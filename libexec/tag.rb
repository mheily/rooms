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
    require 'json'
    require 'pp'
    require 'securerandom'
  
    require_relative 'log'
    
    def initialize(room, parsed_json)
      @room = room
      @data = parsed_json
      @uuid = @data['uuid']
    end
    
    def name ; @data['name'] ; end
      
    # Given a simple ZFS snapshot, create a tag
    def self.from_snapshot(snapshot_name, room)
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
      outfile = room.mountpoint + '/tags/' + meta['uuid']
      snapshot_ref = "#{room.dataset}/share@#{snapshot_name}"
      system("/sbin/zfs send #{incremental_opts} #{snapshot_ref} > #{outfile}.raw") or raise "zfs send failed"
      system("xz < #{outfile}.raw > #{outfile}") or raise 'xz failed'
      File.unlink(outfile + '.raw')
      meta['sha512'] = `sha512 -q #{outfile}`.chomp
      raise 'sha512 failed' if $? != 0
      
      File.open("#{outfile}.json", "w") { |f|
        f.puts JSON.pretty_generate(meta)
      }
 
      meta
    end
  
    # @param scp [Net::SCP] a session
    # @param remote_path [String] the remote directory where the room is stored
    # @param destdir [String] the directory to download the tag to   
    def fetch(scp: nil, remote_path: nil, destdir: nil)
      raise ArgumentError unless scp && remote_path && destdir
      src = remote_path + '/tags/' + @uuid
      dst = destdir + '/tags'
      logger.info "Dowloading #{src} to #{dst}"
      data = scp.download!(src, dst)
    end

    # @param scp [Net::SCP] a session
    # @param remote_path [String] the remote directory where the room is stored
    # @param destdir [String] the directory to download the tag to   
    def upload(scp: nil, remote_path: nil)
      raise ArgumentError unless scp && remote_path
      tagdir = @room.mountpoint + '/tags' 
      [@data['uuid'], @data['uuid'] + '.json'].each do |filename|
        src = tagdir + '/' + filename
        dst = remote_path + '/tags/' + filename
        logger.info "Uploading #{src} to #{dst}"
        data = scp.upload!(src, dst)
      end
    end    
    
  end
end