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
    private
    require 'json'
    require 'pp'
    
    require_relative 'tag'
    require_relative 'log'
    require_relative 'platform'

    attr_reader :exp
    
    public
    
    attr_reader :tagdir
    
    def initialize(roomdir)
      raise ArgumentError unless roomdir.is_a?(String)
      @roomdir = roomdir
      @tagdir = roomdir + '/tags'
      @dataset = `df -h #{roomdir}/share | tail -1 | awk '{ print \$1 }'`.chomp
      @platform = Room::Platform.new
      @tags = nil
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
        logger.debug "building the list of tags"
        refresh
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
    def construct(room)
      result = {'api' => { 'version' => 0 }, 'tags' => []}
      snapshots.each do |snapshot|
        tagfile = tagdir + '/' + snapshot + '.tag'
        metadatafile = tagdir + '/' + snapshot + '.json'
        if File.exist? tagfile
          buf = File.open(metadatafile, 'r').readlines.join('')
          result['tags'] << JSON.parse(buf)
        else
          meta = Room::Tag.from_snapshot(snapshot, room)
          result['tags'] << meta
        end
      end
      # DEADWOOD: try to avoid this
      #File.open(indexfile, 'w') { |f| f.puts JSON.pretty_generate(result) }
    end
    
    def parse(json)
      begin
      @json = JSON.parse(json)
      rescue => e
        puts "error in JSON: #{json}"
        raise e
      end
    end

    # @param remote_path [String] path to the remote room       
    def connect(exp)
      raise TypeError unless exp.is_a? UriExplorer
      @exp = exp
      @remote_path = 'tags/' + @platform.to_s
      tags.each do |tag|
        tag.connect(exp, @remote_path)
      end
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
      
      snaplist = snapshots
      exp.ls(@remote_path).each do |ent|
        if ent.name =~ /\.json\z/
          snapname = ent.name.sub(/\.json\z/, '')
          if snaplist.include? snapname
            logger.debug 'skipping ' + snapname
          else
            logger.debug 'downloading ' + snapname
            tag = Room::Tag.new(tagdir, snapname)
            tag.connect(exp, @remote_path)
            tag.fetch
          end
        end
      end
      recv_tags
    end
  
    # After tags are downloaded, call "zfs recv" to apply them to the
    # existing dataset
    def recv_tags
      # Examine all tags, to determine the order to apply them
      tag_order = []
      Dir.glob(tagdir + '/*.json') do |path|
        puts path
      end
      raise 'hello'
    end
    
    def push
      # DEADWOOD: avoid this
      #destfile =  @remote_path + '/index.json'
      #logger.debug "uploading index to #{destfile}"
      #@sftp.upload!(indexfile, destfile)
      
      refresh # WORKAROUND: somehow the cache gets stale
      logger.debug 'pushing tags'
      tags.each { |tag| 
        tag.connect(exp, @remote_path)
        tag.push
      }
      logger.debug 'push complete'
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

    # Refresh the list of tags
    def refresh
      @tags = []
      Dir.glob("#{@tagdir}/*.json").each do |path|
        next if File.basename(path) == 'index.json'
        logger.debug "parsing #{path}"
        buf = File.open(path, 'r').readlines.join("\n")
        parsed_json = JSON.parse(buf)
        tag = Room::Tag.new(@tagdir, parsed_json['name'])
        tag.parse(parsed_json)
        @tags << tag
      end
    end

    # A list of ZFS snapshots associated with the room
    def snapshots
      # Strangely, this only lists the first snapshot. If you remove '-o name', it lists them all
      #command = "zfs list -H -r -t snapshot -o name #{@dataset}/share"
      command = "ls -1 #{@roomdir}/share/.zfs/snapshot"
      logger.debug "running: #{command}"
      `#{command}`.chomp.split(/\n/).map { |x| x.sub(/.*@/, '') }
    end
    
    # If we are connected to a remote index
    def connected?
      exp ? true : false
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