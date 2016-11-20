#
# Copyright (c) 2016 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# A snapshot of a room
class Room
  class Tag
    private
    
    require 'json'
    require 'pp'
    require 'securerandom'
    
    require_relative 'log'
    require_relative 'platform'    
    
    attr_accessor :exp
    
    public
        
    attr_accessor :tagdir, :logger
    
    def initialize(tagdir, name)
      raise ArgumentError, tagdir unless File.exist? tagdir
      @data = nil
      @tagdir = tagdir
      @name = name
      @platform = Platform.new
      
      @logger = Room::Log.instance.logger
      #@room = room
      #@data = parsed_json
      #@uuid = @data['uuid']
      #@tagdir = room.mountpoint + '/tags'
    end
    
    def parse(h)
      raise ArgumentError, h.class.name unless h.is_a? Hash
      @data = h
      raise 'name mismatch' unless @name == @data['name'] 
      @uuid = @data['uuid']
      @logger.debug "parsed: #{@data.pretty_inspect}"
    end
      
    def name ; @data['name'] ; end
      
    # Given a simple ZFS snapshot, create a tag
    def self.from_snapshot(snapshot_name, room)
      raise 'bad dataset' unless room.dataset
      
      meta = { 
        'name' => snapshot_name,
        'uuid' => SecureRandom.uuid,
        'format' => 'zfs',
        'compression' => 'xz',
        'zfs' => Hash.new,
      }
      
      all_tags = room.tags
      if snapshot_name == all_tags[0]
        meta['zfs']['incremental_source'] = ''
        meta['zfs']['origin'] = room.dataset_origin
        incremental_opts = ''
      else
        meta['zfs']['incremental_source'] = room.previous_snapshot(snapshot_name)
        incremental_opts = '-i ' + meta['zfs']['incremental_source']
      end
      outfile = room.mountpoint + '/tags/' + meta['name']
      snapshot_ref = "#{room.dataset}/share@#{snapshot_name}"
      system("/sbin/zfs send #{incremental_opts} #{snapshot_ref} > #{outfile}.raw") or raise "zfs send failed"
      # TODO: support other compression engines
      if false
        system("xz < #{outfile}.raw > #{outfile}.tag") or raise 'xz failed'
        File.unlink(outfile + '.raw')
      else
        meta['compression'] = 'none'
        system "mv #{outfile}.raw #{outfile}.tag" or raise 'mv failed'
      end
      
      meta['sha512'] = `sha512 -q #{outfile}.tag`.chomp
      raise 'sha512 failed' if $? != 0
      
      File.open("#{outfile}.json", "w") { |f|
        f.puts JSON.pretty_generate(meta)
      }
 
      meta
    end
  
    def connect(exp)
      raise TypeError unless exp.is_a? UriExplorer
      @exp = exp
    end
    
    # Does the tag exist on the local host?
    def exist?
      File.exist? datafile
    end
    
    def fetch
      raise 'not connected' unless exp

      src = remote_tagdir + '/' + @name + '.json'
      logger.info "Downloading #{src} to #{metadatafile}"
      data = exp.download(src, metadatafile)
      load_metadata
      
      src = remote_tagdir + '/' + @name + '.tag'
      logger.info "Downloading #{src} to #{datafile}"
      data = exp.download(src, datafile)
    end
 
    def push
      remote_tags = exp.ls(remote_tagdir)
      logger.debug "remote_tags=#{remote_tags.inspect}"
      [name + '.tag', name + '.json'].each do |filename|
        src = @tagdir + '/' + filename
        dst = remote_tagdir + '/' + filename
        if remote_tags.include?(File.basename(dst))
          logger.info "Skipping #{dst}; it already exists"
        else
          logger.info "Uploading #{src} to #{dst}"
          exp.upload(src, dst + ".tmp")
          exp.rename(dst + ".tmp", dst)
        end
      end
    end    
    
    private
    
    # The directory containing tags on the remote origin
    def remote_tagdir
      'tags/' + @platform.to_s
    end
    
    # The file containing the actual tag data 
    def datafile
      @tagdir + '/' + @name + '.tag'
    end
    
    # The file containing the tag metadata 
    def metadatafile
      @tagdir + '/' + @name + '.json'
    end

    # Load metadata
    def load_metadata
      logger.debug 'loading ' + metadatafile
      buf = File.open(metadatafile, 'r').readlines.join('')
      logger.debug "buf=#{buf}"
      parse(JSON.parse(buf))
    end
  end
end