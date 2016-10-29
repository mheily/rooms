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
  base_uri = ARGV[0]
  name = ARGV[1] || base_uri.gsub(/.*\//, '')
  raise "usage: #{$PROGRAM_NAME} <uri or name> [name]" unless base_uri
  
  setup_logger
  #setup_tmpdir

  if base_uri =~ /^http|ssh/
    logger.debug "cloning: base_uri=#{base_uri} name=#{name}"

    base_uri = URI(base_uri)
    room = RemoteRoom.new(base_uri, logger)
    room.connect
    room.clone(name)
  else
    system('room', name, 'create', '--clone', base_uri) or raise 'failed to create room'
  end
    
  logger.debug 'done'
end

main