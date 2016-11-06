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
class TagIndex
  require 'json'
  require 'pp'
  
  require_relative 'tag'
  
  attr_accessor :scp
  
  # @param scp [Net::SCP] open connection to the remote server
  # @param remote_path [String] path to the remote room
  def initialize(json: nil, scp: nil, remote_path: nil)
    parse(json) if json
    @scp = scp
    @remote_path = remote_path
    fetch if @scp and @remote_path
  end
  
  # Construct an index for a local room
  def construct(room)
    raise ArgumentError unless room.kind_of?Room
    indexfile = room.mountpoint + '/etc/tags.json'
    if File.exist?(indexfile)
      raise indexfile + ': already exists'
    else
      system "rm -f #{room.mountpoint}/tags/*"
      result = {'api' => { 'version' => 0 }, 'tags' => []}
      room.tags.each do |tag|
        meta = Room::Tag.from_snapshot(tag, room)
        result['tags'] << meta
      end
      File.open(indexfile, 'w') { |f| f.puts JSON.pretty_generate(result) }
    end
  end
  
  def parse(json)
    @json = JSON.parse(json)
  end
  
  def fetch
    data = @scp.download!(@remote_path + '/tags.json')
    @json = JSON.parse(data)
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
  def delete(name: nil, uuid: nil)
    raise ArgumentError if name && uuid
    raise ArgumentError, 'UUID not implemented' unless name
    new_tags = [@json['tags'].find { |ent| ent['name'] != name }].flatten
    @json['tags'] = new_tags
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