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

def main
  path = ARGV[0]
  raise 'usage: room-build <configuration file>' unless path
  raise Errno::ENOENT unless File.exist?(path)
  
  @logger = Logger.new(STDOUT)
  @logger.level = Logger::DEBUG
  @cachedir = '/tmp'
  
  data = `uclcmd get --file #{path} -c ''`
  json = JSON.parse data
  logger.debug "json=#{json}"
  archive = @cachedir + '/' + json['label'] + '-base.txz'
  unless File.exist?(archive)
    system("fetch -o #{archive} #{json['base']['uri']}") or raise 'fetch failed'    
  end
  system("sha512 -c #{json['base']['sha512']} #{archive} >/dev/null") or raise 'checksum mismatch'
  system "room #{json['label']} create --uri=file://#{archive}"
  
  json['base']['script'].each do |command|
    system "room #{json['label']} exec -u root -- #{command}"
  end
  
  system "room #{json['label']} snapshot #{json['base']['tag']} create"
end
    
def logger
  @logger
end

def system(command)
  logger.debug 'running: ' + command
  Kernel.system(command)
end

main

__END__
{
  "api": {
    "version": 0
  },
  "label": "com.heily.FreeBSD-11.0",
  "name": "FreeBSD-11.0",
  "base": {
    "uri": "ftp://ftp.freebsd.org/pub/FreeBSD/releases/amd64/amd64/11.0-RELEASE/base.txz",
    "sha512": "137e84cc8774729bb55349e43de5df55bf8b50dc37eb4fb6b4f4f2b9f94daf11dd2841db2d6b8be0d081766d665068f97f47bdae73f23809588ae75a2d43a476",
    "type": "archive",
    "tag": "base",
    "script": [
      "env ASSUME_ALWAYS_YES=YES pkg bootstrap",
      "sed -i -e 's/enabled: no/enabled: yes/' /etc/pkg/FreeBSD.conf",
      "pkg update"
    ]
  },
  "tags": [
    {
      "name": "v0.1",
      "script": [
        "cd /tmp2",
        "git fetch",
        "git checkout tags/:v0.1",
        "make all install"
      ]
    },
    {
      "name": "v0.2",  
      "script": [
        "cd /tmp3",
        "git fetch",
        "git checkout tags/:v0.1",
        "make all install"
      ]
    }
  ]
}