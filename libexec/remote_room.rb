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
  require 'net/ssh'
  require 'uri'
    
  attr_reader :uri, :name, :path, :tags
  
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
    @ssh = Net::SSH.start(host)
    fetch
  end
  
  def clone(local_name)
    local_name = @name if local_name.empty?
    
    tags_copy = tags.dup
    
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
    @options_json = download_json "#{@path}/options.json"
    @tags_json = download_json "#{@path}/tags.json"
  end
   
  def logger
    @logger
  end

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
  
  private
  
  def download_json(path)
    logger.debug "downloading #{path}"
    json = @ssh.exec!("cat #{Shellwords.escape(path)}") # XXX-error checking
    logger.debug "got: #{json}"
    return JSON.parse(json)
  end
  
  def tags
      @tags_json['tags'].map { |ent| ent['name'] }
  end
  
  def is_clone?
    @options_json['template']['uri'] != ''
  end
  
  # The name of the template
  # KLUDGE: this forces the local room name to match the remote
  def template_name
    @options_json['template']['uri'].sub(/.*\//, '')
  end

  def template_tag
    @options_json['template']['snapshot']
  end
    
  # Download the base template, if it does not already exist
  def download_template
    uri = @options_json['template']['uri']
  
    if Room.exist? template_name
      logger.debug "template #{template_name} already exists"
    else
      logger.debug "downloading base template #{template_name} from #{uri}"
      system('room', 'clone', uri) or raise "failed to clone template"
    end
  end
end