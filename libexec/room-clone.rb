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
require 'tempfile'
require 'uri'
require_relative 'room'

include RoomUtility

def main
  user = `whoami`.chomp
  
  base_uri = RoomURI.new(ARGV[0])

  if ARGV[1].empty?
    name = base_uri.name
  else
    name = ARGV[1]
  end

  template_tag = ARGV[2]
  raise "usage: #{$PROGRAM_NAME} <uri or name> [name] [tag]" unless base_uri

  uri = base_uri.uri
  room = Room.new(name)
  
  if %w(file http https ssh).include? uri.scheme
    logger.debug "cloning: uri=#{uri} name=#{name}"

    begin
      room.options.origin = uri
      room.clone
    rescue => e
      logger.error "caught an exception in Room.clone"
      logger.debug "destroying partially cloned room #{name}"
      system "room #{name} destroy >/dev/null 2>&1"
      raise e
    end

  elsif uri.scheme == 'file' || (uri.scheme == 'room' and uri.host == 'localhost')
    raise 'DEADWOOD'
    
    src_room = Room.new(uri.path)
    src_name = uri.path.sub(/^\//, '')
    logger.debug "cloning #{src_name}"
    args = ['room', name, 'create', '--clone', src_name]
    if template_tag
      args << '--tag'
      args << template_tag
    end
    system(*args) or raise 'failed to create room'
    dst_room = Room.new(name, logger)
    dst_room.uuid = `uuid -v4`.chomp
    dst_room.template_uri = src_room.origin
    dst_room.template_snapshot = src_room.tags.dup.pop
    dst_room.origin = ""
  else
    raise 'Invalid URI: ' + uri
  end
    
  logger.debug 'done'
end

main
