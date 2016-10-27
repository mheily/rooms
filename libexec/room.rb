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

# A room on a remote server
class RemoteRoom
  attr_reader :uri, :name, :path, :tags
  
  def initialize(uri, logger)
    @uri = URI(uri)
    @name = @uri.path.sub(/.*\//, '')
    @path = @uri.path.sub(/^\/~\//, './')  # support ssh://$host/~/foo notation
    @logger = logger
    logger.debug "initialized; name=#{@name} uri=#{@uri} path=#{@path}"
  end

  def connect
    host = uri.host
    raise 'host missing' unless host
    logger.debug "connecting to #{host}"
    @ssh = Net::SSH.start(host)
    fetch
  end
  
  # Get information about the remote room
  def fetch
    @options_json = download_json "#{@path}/options.json"
    @tags_json = download_json "#{@path}/tags.json"
  end
  
  def tags
    @tags_json['tags'].map { |ent| ent['name'] }
  end
  
  def logger
    @logger
  end

  def download_tag(name, local_room_name)   
    logger.debug "downloading tag '#{name}'"  
    @ssh.open_channel do |channel|
      channel.exec("cat #{@path}/tags/#{name}.zfs.xz") do |ch, success|
        raise 'command failed' unless success
        
        command = "xz -d | room #{local_room_name} snapshot #{name} receive"
        
        logger.debug("popen command: #{command}")
        zfs = IO.popen(command, "w")
        
        channel.on_data do |ch, data|
          zfs.write(data)
        end
      end
    end
  
    @ssh.loop
    logger.debug 'tag downloaded successfully'
  end
  
  private
  
  def download_json(path)
    logger.debug "downloading #{path}"
    json = @ssh.exec!("cat #{Shellwords.escape(path)}") # XXX-error checking
    logger.debug "got: #{json}"
    return JSON.parse(json)
  end
end

# A room on localhost
class Room
  attr_reader :name, :mountpoint, :dataset, :tags
  
  def initialize(name)
    @name = name
    user = `whoami`.chomp
    @mountpoint = "/room/#{user}/#{name}"
    @dataset = `df -h /room/mark/FreeBSD-11.0-RELEASE/ | tail -1 | awk '{ print \$1 }'`.chomp
    @json = JSON.parse options_json
  end
  
  def tags
    `zfs list -H -r -t snapshot -o name #{@dataset}/share`.chomp.split(/\n/).map { |x| x.sub(/.*@/, '') }
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
    @json['instance']['originUri']
  end
end

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
  
  def system(command)
    logger.debug 'running: ' + command
    Kernel.system(command)
  end
  
end
