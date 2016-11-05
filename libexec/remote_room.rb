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
class RemoteRoom
  require 'json'
  require 'logger'
  require 'net/scp'
  require 'uri'

  require_relative 'room_options' 
  require_relative 'tag_index'

  attr_reader :uri, :name, :path
  
  def initialize(uri: nil, local_name: nil, logger: nil, tmpdir: nil)
    raise 'invalid usage' unless uri and local_name and logger and tmpdir
    @uri = URI(uri)
    @local_name = local_name
    @user = `whoami`.chomp
    @name = @uri.path.sub(/.*\//, '')
    @path = @uri.path.sub(/^\/~\//, './')  # support ssh://$host/~/foo notation
    @logger = logger
    @tmpdir = tmpdir
    logger.debug "initialized; name=#{@name} uri=#{@uri} path=#{@path}"
  end

  def connect
    host = uri.host
    raise 'host missing' unless host
    logger.debug "connecting to #{host}"
    @scp = Net::SCP.start(host, @user)
    fetch
  end
  
  def clone(local_name)
    local_name = @name if local_name.empty?
    
    tags_copy = @tag_index.names.dup
    
    if is_clone?
      logger.info "Cloning #{local_name} from the local template #{template_name}"

      download_template
      args = ['room', local_name, 'create', '--clone', template_name, '--tag', template_tag]
      args << '-v' if ENV['ROOM_DEBUG']
      system(*args) or raise "unable to create room"
      #TESTING: tags_copy.shift # The first tag comes from the template, not the remote room.

      template_dataset = `df -h /room/#{@user}/#{template_name} | tail -1 | awk '{ print \$1 }'`.chomp

      local_room = Room.new(local_name, logger)
    else
      logger.info "Creating an empty room named #{local_name}"
      system('room', local_name, 'create', '--empty') or raise "unable to create room"
    end
    
    tags_copy.each do |tag|
      download_tag(tag, local_name)
    end
    
    room = Room.new(local_name, logger)
    room.origin = @uri.to_s
  end
  
  # Get information about the remote room
  def fetch
    @options = RoomOptions.new(scp: @scp, remote_path: @path)
    @tag_index = TagIndex.new(scp: @scp, remote_path: @path)
  end
   
  def logger
    @logger
  end
  
  private
    
  def is_clone?
    @options['template']['uri'] != ''
  end
  
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