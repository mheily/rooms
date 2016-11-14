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

def main
  user = `whoami`.chomp
  room_name, origin = ARGV[0], ARGV[1].dup
  raise "usage: #{$PROGRAM_NAME} <room> [origin]" unless room_name
  
  room = Room.new(room_name)
  room.options.origin = origin if !origin.empty?
  room.push
  logger.debug 'push complete'
end
    

main
