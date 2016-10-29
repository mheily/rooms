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
require 'net/http'
require 'net/ssh'
require 'shellwords'
require 'tempfile'
require 'uri'
require_relative 'room'

include RoomUtility

def upload_tag(ssh, room, index, tagfile)
  tags = room.tags
  tag = tags[index]
  logger.debug "uploading tag: #{tag}"
  tagfile = Shellwords.escape(tagfile)
  
  # TODO: delete the temporary file if the upload is aborted
  
  ssh.open_channel do |channel|
    channel.exec("cat > #{tagfile}.tmp#{$$}") do |ch, success|
      raise 'command failed' unless success
      
      # If this is the first snapshot in the list, send the entire stream
      # Otherwise, send an incremental stream
      if index == 0
        command = "zfs send #{room.dataset}/share@#{tag}"
      else
        command = "zfs send -i #{room.tags[index - 1]} #{room.dataset}/share@#{tag}"
      end
      command += ' | xz'
      
      logger.debug("popen command: #{command}")
      zfs = IO.popen(command)
      while !zfs.eof?
        channel.send_data(zfs.read(1024**2))
      end
      zfs.close
      unless $?.to_i == 0
        logger.error "zfs send failed"
        raise 'Failed to send tag'
      end

      channel.eof!
      channel.close
    end
  end

  ssh.loop
  ssh.exec!("mv #{tagfile}.tmp#{$$} #{tagfile}.zfs.xz")
  logger.debug 'tag uploaded successfully'
end

# FIXME: uri is modified by this function, would be better to have a #canonicalize_uri method do this 
def push_via_ssh(room_name, uri)
  user = `whoami`.chomp
  
  room = Room.new(room_name, logger)
  
  if uri.nil? or uri.empty?
    uri = room.origin 
    if uri.nil?
      logger.error "No origin URI has been defined for this room"
      exit 1
    end
  end
   
  if room.tags.empty?
    raise "Room has no tags. You must create at least one tag before pushing."
  end

  Net::SSH.start(URI(uri).host) do |ssh|
    
    if uri =~ /\/~\//
      logger.debug "converting ~ into a real path; uri=#{uri}"
      remote_home = ssh.exec!('echo $HOME').chomp
      uri.sub!(/\/~\//, remote_home + '/')
      logger.debug "remote home is #{remote_home}, new URI is #{uri}"
    end
  
    room.origin = uri
    uri = URI(uri)
    path = uri.path
  
    raise "unsupported scheme; uri=#{uri}" unless uri.scheme == 'ssh'
    puts "pushing room #{room.name} to #{uri.to_s}"
      
    basedir = path
    logger.debug "creating #{basedir} directory tree"
    ssh.exec!("install -d -m 755 #{Shellwords.escape(basedir)} #{Shellwords.escape(basedir + '/tags')}")
    
    logger.debug "uploading options.json"
    ssh.exec!("echo #{Shellwords.escape room.options_json} > #{Shellwords.escape(basedir)}/options.json")

    logger.debug "getting tags from remote server"
    remote_tags_path = Shellwords.escape(basedir) + "/tags.json"
    buf = ssh.exec!("test -e #{remote_tags_path} && cat #{remote_tags_path}")
    if buf.empty?
      remote_tags = {'tags'=> []}
    else
      remote_tags = JSON.parse(buf)
    end
    logger.debug "remote_tags=#{remote_tags.inspect}"
     
    remote_tag_names = remote_tags['tags'].map { |ent| ent['name'] }
    i = 0
    refresh = false
    room.tags.each do |tag|
      if remote_tag_names.include? tag
        logger.debug "tag #{tag} already exists; skipping"
      else
        tagfile = basedir + '/tags/' + tag
        upload_tag(ssh, room, i, tagfile)
        refresh = true
      end
      i += 1
    end
    
    if refresh
      logger.debug "uploading tags.json: #{room.tags_json}"
      ssh.exec!("echo #{Shellwords.escape room.tags_json} > #{Shellwords.escape(basedir)}/tags.json")
    end

  end
end

def main
  user = `whoami`.chomp
  room, origin = ARGV[0], ARGV[1].dup
  raise "usage: #{$PROGRAM_NAME} <room> [origin]" unless room
  
  setup_logger
  #setup_tmpdir

  push_via_ssh room, origin
  logger.debug 'push complete'
end
    

main
