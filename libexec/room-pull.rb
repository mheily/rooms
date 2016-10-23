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

def main
  user = `whoami`.chomp
  name = ARGV[0]
  raise 'usage: room-pull <name>' unless name
  
  @logger = Logger.new(STDOUT)
  @logger.level = Logger::DEBUG
  @cachedir = '/tmp' # XXX-FIXME SECURITY
  
  data = `uclcmd get --file /room/#{user}/#{name}/etc/options.json -c ''`
  json = JSON.parse data
  logger.debug "json=#{json.pretty_inspect}"

  uri = URI(json['instance']['originuri'] + '/options.json')
  uuid = json['instance']['uuid']
  logger.debug "fetching #{uri}"
  case uri.scheme
  when 'ssh'
    options = JSON.parse(`ssh #{uri.host} cat #{uri.path}`)
    if options['instance']['uuid'] != uuid
      raise 'TODO -- support replacing the entire room'
    end
    pp options
    
  when 'http', 'https'
    raise 'todo'; #data = `fetch -o - #{uri}`
  else
    raise 'unsupported scheme: ' + uri.scheme
  end
  
  current_tags = `room #{name} snapshot list`.split(/\n/)
  logger.debug "room #{name} current_tags=#{current_tags.inspect}" 
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
    
def logger
  @logger
end

def system(command)
  logger.debug 'running: ' + command
  Kernel.system(command)
end

main
