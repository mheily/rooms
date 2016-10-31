#!/usr/bin/env ruby
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

require "json"
require "pp"
require 'logger'
require 'tempfile'
require 'net/ssh'
require 'shellwords'

#
# Utility functions for working with rooms
#
module RoomUtility
  def setup_logger
    @logger = Logger.new(STDOUT)
    if ENV['ROOM_DEBUG']
      @logger.level = Logger::DEBUG
    else
      @logger.level = Logger::INFO
    end
  end
  
  def setup_tmpdir
    @tmpdir = Dir.mktmpdir($PROGRAM_NAME.sub(/.*\//, ''))
    at_exit { system "rm -rf #{@tmpdir}" if File.exist?(@tmpdir) }
  end
    
  def logger
    @logger
  end
  
  def parse_options_json(path)
    data = `uclcmd get --file #{path} -c ''`
    json = JSON.parse data
    logger.debug "json=#{json.pretty_inspect}"
    json
  end
  
  def system(*args)
    logger.debug 'running: ' + args.join(' ')
    Kernel.system(*args)
  end
  
end

# An URI of a Room
class RoomURI
  
  attr_reader :uri
  
  def initialize(uri)
    @original = uri
    @canonical = canonicalize(uri)
    @uri = URI(@canonical)
  end
  
  private
  
  def canonicalize(s)
    if s =~ /:\/\//
      s
    else
      'room://localhost/' + s 
    end
  end
end

# A room on a remote server
class RemoteRoom
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

## The options.json for a room
#class RoomOptions
#  def initialize(path)
#    @path = path
#    @json = JSON.parse(File.read(path))
#    pp json
#    raise 'hi'
#  end
#end

# A room on localhost
class Room
  attr_reader :name, :mountpoint, :dataset, :dataset_origin, :tags
  
  include RoomUtility
  
  def initialize(name, logger)
    @name = name
    @user = `whoami`.chomp
    @mountpoint = "/room/#{@user}/#{name}"
    @dataset = `df -h /room/#{@user}/#{name} | tail -1 | awk '{ print \$1 }'`.chomp
    @dataset_origin = `zfs get -Hp -o value origin #{@dataset}/share`.chomp
    @json = JSON.parse options_json
    @logger = logger
  end
  
  def Room.exist?(name)
    File.exist? "/room/#{`whoami`.chomp}/#{name}"
  end
  
  def tags
    # Strangely, this only lists the first snapshot. If you remove '-o name', it lists them all
    #command = "zfs list -H -r -t snapshot -o name #{@dataset}/share"
    command = "ls -1 #{@mountpoint}/share/.zfs/snapshot"
    logger.debug "running: #{command}"
    `#{command}`.chomp.split(/\n/).map { |x| x.sub(/.*@/, '') }
  end
  
  def is_clone?
    @json['template']['uri'] != ''
  end
  
  def has_tag?(name)
    tags.each do |tag|
      return true if tag == name
    end
    return false
  end
  
  def tags_json
    names = tags
    nameset = tags.map do |name|
      { 'name' => name, 'compression' => 'xz', 'format' => 'zfs' }
    end
    
    JSON.pretty_generate({
      'api' => { 'version' => 0 },
      'tags' => nameset
    })
  end
  
  def options_json
    `cat #{mountpoint}/etc/options.json`.chomp
  end
  
  def origin
    @json['remotes']['origin']
  end
  
  def origin=(uri)
    @json['remotes']['origin'] = uri
    save_options
  end
  
  def template_uri=(s)
    @json['template']['uri'] = s
    save_options  
  end
  
  def template_snapshot=(s)
    @json['template']['snapshot'] = s
    save_options  
  end
  
  def uuid=(s)
    @json['uuid'] = s
    save_options  
  end
  
  # Create a ZFS replication stream and save it at [+path+]
  def create_tags_archive(path)
    cmd = [ 'zfs', 'send', '-v' ]
    if @dataset_origin == '-'
      # This room is not a ZFS clone
      if oldest_snapshot == current_snapshot
        cmd << current_snapshot
      else
        cmd << ['-I', oldest_snapshot, current_snapshot]
      end
    else
      raise 'TODO: send incremental based on origin snapshot'
    end
    cmd << [ '>', path]
    cmd.flatten!
    cmd = cmd.join(' ') #FIXME: workaround for weird issue when array is passed
    system(cmd) or raise 'send operation failed'
  end
  
  # The most recent snapshot of the 'share' dataset
  def current_snapshot
    `zfs list -r -H -t snapshot -o name -S creation #{@dataset}/share | head -1`.chomp
  end

  # The oldest snapshot of the 'share' dataset
  def oldest_snapshot
    `zfs list -r -H -t snapshot -o name -S creation #{@dataset}/share | tail -1`.chomp
  end
    
  private
  
  def save_options
    File.open("#{mountpoint}/etc/options.json", "w") do |f|
      f.puts JSON.pretty_generate(@json)
    end
  end
  
end

