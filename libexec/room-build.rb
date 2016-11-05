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
require_relative 'room'

include RoomUtility

def main
  path = ARGV[0]
  raise 'usage: room-build <configuration file>' unless path
  raise Errno::ENOENT unless File.exist?(path)
  
  setup_logger
  setup_tmpdir
  
  data = `uclcmd get --file #{path} -c ''`
  json = JSON.parse data
  logger.debug "json=#{json}"

  @label = json['label']
  if `room list`.split(/\n/).grep(/\A#{@label}\z/).length > 0
  	logger.debug "room #{@label} exists"
  else
  	create_base(json)
  end
  
  current_tags = `room #{json['label']} snapshot list`.split(/\n/)
  logger.debug "current_tags=#{current_tags.inspect}" 
  json['tags'] ||= []
  json['tags'].each do |tag|
  	if current_tags.include?(tag['name'])
  	  logger.debug "tag #{tag['name']} already exists"
  	else
  	  logger.debug "creating tag: #{tag['name']}"
  	  run_script tag['script'] if tag.has_key? 'script'
  	  system "room #{@label} snapshot #{tag['name']} create"
  	end
  end
  
  logger.debug 'done'
end

main