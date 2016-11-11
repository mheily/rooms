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

# A data structure containing metadata for all of the tags of a room
class Room
  class TagIndex
    require 'json'
    require 'pp'
    
    require_relative 'tag'
    require_relative 'log'
    require_relative 'platform'

    attr_reader :tagdir
    
    def initialize(roomdir)
      raise ArgumentError unless roomdir.is_a?(String)
      @tagdir = roomdir + '/tags'
    end
    
    def load_file
      logger.debug "loading index from #{indexfile}"
      raise Errno::ENOENT,indexfile unless File.exist?(indexfile)
      buf = File.open(indexfile, 'r').readlines.join
      logger.debug "loaded; buf=#{buf}"
      parse(buf)
    end
    
    def tags
      if @tags.nil?
        @tags = []
        @json['tags'].each do |ent|
          tag = Room::Tag.new(@tagdir)
          tag.parse(ent)
          @tags << tag
        end
      end
      @tags
    end
    
    def tag(opts)
      raise ArgumentError unless opts.is_a? Hash     
      if opts[:name]
        tags.each { |x| return x if x.name == opts[:name] }
      elsif opts[:name]
        raise 'todo'
      end
      raise Errno::ENOENT
    end
    
    # Construct an index for a local room
    def construct   
      if File.exist?(indexfile)
        logger.debug "skipping; index already exists"
      else
        result = {'api' => { 'version' => 0 }, 'tags' => []}
        room.tags.each do |tag|
          meta = Room::Tag.from_snapshot(tag, @room)
          result['tags'] << meta
        end
        File.open(indexfile, 'w') { |f| f.puts JSON.pretty_generate(result) }
      end
    end
    
    def parse(json)
      begin
      @json = JSON.parse(json)
      rescue => e
        puts "error in JSON: #{json}"
        raise e
      end
    end

    # @param sftp [Net::sftp] open connection to the remote server
    # @param remote_path [String] path to the remote room       
    def connect(sftp, remote_path)
      @sftp = sftp
      @remote_path = remote_path
    end
    
    # Download the index and any new tags
    def fetch
      raise 'not connected' unless connected?
      
      # TODO: avoid using index.json and look at all the tags via SFTP
      # Download all new tags
#      @sftp.dir.foreach(path) do |entry|
#        next if %w(. ..).include?(entry.name)
#        puts entry.name
#      end
      
      data = @sftp.download!(@remote_path + '/tags/index.json')
      @json = JSON.parse(data)
      logger.debug "writing new #{indexfile}"
      File.open(indexfile, 'w') { |f| f.puts data }
      tags.each do |tag|
        tag.connect(@sftp, @remote_path)
        tag.fetch unless tag.exist?
      end
    end
  
    def push
      @sftp.upload!(indexfile, @remote_path + '/tags/index.json')
    end
      
    # The names of all tags
    def names
      @json['tags'].map { |ent| ent['name'] }
    end
    
    def to_s
      @json['tags'].pretty_inspect
    end
    
    # Delete a tag from the index.
    # Does not actually touch the ZFS snapshot.
    def delete(opts)
      raise ArgumentError unless opts.is_a?(Hash)
      name = opts[:name]
      new_tags = [@json['tags'].find { |ent| ent['name'] != name }].flatten
      @json['tags'] = new_tags
    end
    
    private

    # If we are connected to a remote index
    def connected?
      @sftp ? true : false
    end
    
    def indexfile
      @tagdir + '/index.json'
    end
        
    def logger
      Room::Log.instance.logger
    end
  end
end
__END__

def download_tags
  remote_path = "#{@path}/tags.zfs.xz"
  archive = "#{@tmpdir}/tags.zfs.xz"
  
  logger.debug "downloading #{remote_path} to #{archive}"
  f = File.open(archive, 'w')
  @ssh.open_channel do |channel|
    channel.exec("cat #{Shellwords.escape remote_path}") do |ch, success|
      raise 'command failed' unless success
             
      channel.on_data do |ch, data|
        f.write(data)
      end
      
      channel.on_close do |ch, data|
        f.close
      end
    end
  end

  @ssh.loop

  system('unxz', archive) or raise 'unxz failed'
  archive.sub!(/\.xz\z/, '')
  #TODO zfs recv this
  raise archive
  logger.debug 'tag downloaded successfully'
end