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
require 'uri'
require_relative 'room'

include RoomUtility

def main
  user = `whoami`.chomp
  name = ARGV[0]
  raise 'usage: room-pull <name>' unless name

  setup_logger
  #setup_tmpdir  
  
  room = Room.new(name)
  remote = RemoteRoom.new(room.origin, logger)
  remote.connect
  
  current_tags = room.tags_json
  logger.debug "room #{name} current_tags=#{room.tags.inspect}" 
  remote.tags.each do |tag|
  	if room.has_tag?(tag)
  	  logger.debug "tag #{tag} already exists"
  	else
  	  remote.download_tag(tag, name)
  	end
  end
  
  logger.debug 'done'
end

main
