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

include Room

def main
  user = `whoami`.chomp
  base_uri = ARGV[0]
  name = ARGV[1] || base_uri.gsub(/.*\//, '')
  raise "usage: #{$PROGRAM_NAME} <uri> [name]" unless base_uri
  
  setup_logger
  setup_tmpdir

  logger.debug "cloning: base_uri=#{base_uri} name=#{name}"

  base_uri = URI(base_uri)
  Net::HTTP.start(base_uri.host, base_uri.port) do |http|
    uri = base_uri.to_s + '/options.json'
    logger.debug "getting #{uri}"
    request = Net::HTTP::Get.new uri
  
    response = http.request request # Net::HTTPResponse object
    raise 'bad response' unless response.body
    json = JSON.parse(response.body)
    logger.debug "json=#{json.pretty_inspect}"
  end
 
  raise 'fixme'
  
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
    

main
__END__
void Room::cloneFromOrigin(const string& uri)
{
  string scheme, host, path;

  Room::parseRemoteUri(uri, scheme, host, path);

  createEmpty();

  string tmpdir = roomDataDir + "/local/tmp";
  if (scheme == "http" || scheme == "https") {
    Shell::execute("/usr/bin/fetch", { "-o", roomOptionsPath, uri+"/options.json" });
    Shell::execute("/usr/bin/fetch", { "-o", tmpdir, uri+"/share.zfs.xz" });
  } else if (scheme == "ssh") {
    Shell::execute("/usr/bin/scp", { host + ":" + path + "/options.json", roomOptionsPath });
    Shell::execute("/usr/bin/scp", { host + ":" + path + "/share.zfs.xz", tmpdir });
  } else {
    throw std::runtime_error("unsupported URI scheme");
  }
  Shell::execute("/usr/bin/unxz", { tmpdir + "/share.zfs.xz" });

  // Replace the empty "share" dataset with a clone of the original
  string dataset = roomDataset + "/" + roomName + "/share";
  SetuidHelper::raisePrivileges();
  Shell::execute("/sbin/zfs", { "destroy", dataset });
  Shell::execute("/bin/sh", { "-c", "/sbin/zfs recv -F " + dataset + " < " + tmpdir + "/share.zfs" });
  Shell::execute("/sbin/zfs", {"allow", "-u", ownerLogin, "hold,send", zpoolName + "/room/" + ownerLogin + "/" + roomName });
  SetuidHelper::lowerPrivileges();

  log_debug("clone complete");
}