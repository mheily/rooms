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
require 'net/scp'
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
        if room.is_clone?
          command = "zfs send -I #{room.dataset_origin} #{room.dataset}/share@#{tag}"
        else
          command = "zfs send #{room.dataset}/share@#{tag}"
        end
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

def check_origin(room, uri)
  if uri.nil? or uri.empty?
    uri = room.origin 
    if uri.nil?
      logger.error "No origin URI has been defined for this room"
      exit 1
    end
  end
   
  if room.tags.empty?
    logger.error "Room has no tags. You must create at least one tag before pushing."
    exit 1
  end
  
  logger.debug 'All preflight checks passed'
  return uri
end

def push_via_ssh(room_name, uri)
  user = `whoami`.chomp
  room = Room.new(room_name)
  uri = check_origin(room, uri) # FIXME: uri is modified by this function, would be better to have a #canonicalize_uri method do this 
  
  room.reindex
  
  @ssh = Net::SSH.start(URI(uri).host)

    # DEADWOOD - Bad idea, lets just force fully qualified paths
#    if uri =~ /\/~\//
#      logger.debug "converting ~ into a real path; uri=#{uri}"
#      remote_home = ssh.exec!('echo $HOME').chomp
#      uri.sub!(/\/~\//, remote_home + '/')
#      logger.debug "remote home is #{remote_home}, new URI is #{uri}"
#    end
  
   room.origin = uri
   uri = URI(uri)
   path = uri.path
  
   raise "unsupported scheme; uri=#{uri}" unless uri.scheme == 'ssh'
   puts "pushing room #{room.name} to #{uri.to_s}"
   
   basedir = path
   safe_basedir = Shellwords.escape(basedir)
   
   logger.debug "checking if #{basedir} exists"
   remote_exists = ! @ssh.exec!("ls #{safe_basedir} 2>/dev/null").chomp.empty?
   unless remote_exists
     logger.debug "creating #{basedir} directory tree"
     @ssh.exec!("install -d -m 755 #{safe_basedir} #{safe_basedir + '/tags'}")
   end

   logger.debug "uploading options.json"
   @ssh.scp.upload!(room.mountpoint + '/etc/options.json', "#{basedir}/options.json")

   remote_tags = {'tags'=> []}
   remote_tags_path = basedir + "/tags.json"
   if remote_exists
     logger.debug "getting tags from remote server; path=#{remote_tags_path}"
     buf = @ssh.scp.download!(remote_tags_path)
     remote_tags = JSON.parse(buf)
   else
     logger.debug "newly created remote; no tags exist yet"
   end
   logger.debug "tags: local=#{room.tags.inspect} remote=#{remote_tags.inspect}"
     
   remote_tag_names = remote_tags['tags'].map { |ent| ent['name'] }
   i = 0
   refresh = false
   room.tags.each do |tag|
      
    # Special case: do not upload the first tag if the room is a clone
    if i == 0 and room.is_clone?
        logger.debug "skipping #{tag['name']}; room is a clone"
        i += 1
        next
    end

    if remote_tag_names.include? tag
      logger.debug "tag #{tag} already exists; skipping"
    else
      room.index.tag(name: tag).upload(scp: @ssh.scp, remote_path: basedir)
      refresh = true
    end
    i += 1
  end
    
  if refresh
    logger.debug "uploading tags.json: #{room.tags_json}"
    @ssh.scp.upload!(room.mountpoint + '/etc/tags.json', "#{basedir}/tags.json")
  end
end

def main
  user = `whoami`.chomp
  room, origin = ARGV[0], ARGV[1].dup
  raise "usage: #{$PROGRAM_NAME} <room> [origin]" unless room
  
  #setup_tmpdir

  push_via_ssh room, origin
  logger.debug 'push complete'
end
    

main
