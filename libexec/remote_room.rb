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

# A room on a remote server
class RemoteRoomDEADWOOD
  require 'json'
  require 'logger'
  require 'net/scp'

  require_relative 'log'
  require_relative 'room_options' 
  require_relative 'tag_index'
  require_relative 'room_uri'

  include RoomUtility
  
  attr_reader :uri, :name, :path
  
  def initialize(uri: nil, local_name: nil)
    raise 'invalid usage' unless uri and local_name
    @uri = RoomURI.new(uri).uri
    @local_name = local_name
    @user = `whoami`.chomp
    @name = @uri.path.sub(/.*\//, '')
    @path = @uri.path.sub(/^\/~\//, './')  # support ssh://$host/~/foo notation
    @local_room = Room.new(local_name)
    @local_room.origin = @uri.to_s
    logger.debug "initialized; name=#{@name} uri=#{@uri} path=#{@path}"
  end
  
  # Get information about the remote room
  def fetch
    @options = RoomOptions.new(scp: @scp, remote_path: @path)
    @tag_index = Room::TagIndex.new(@local_room)
    @tag_index.connect(@scp, @path)
    @tag_index.fetch
  end
  
  private
   
  
  # The name of the template
  # KLUDGE: this forces the local room name to match the remote
  def template_name
    @options['template']['uri'].sub(/.*\//, '')
  end

  def template_tag
    @options['template']['snapshot']
  end
    
  # Download the base template, if it does not already exist
  def download_template
    uri = @options['template']['uri']
  
    if Room.exist? template_name
      logger.debug "template #{template_name} already exists"
    else
      logger.debug "downloading base template #{template_name} from #{uri}"
      system('room', 'clone', uri) or raise "failed to clone template"
    end
  end
end