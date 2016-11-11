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

# A data structure containing metadata about the room
class RoomOptions
  require 'json'
  require 'pp'
     
  def initialize(basedir)
    @basedir = basedir
    @json = {
        "api" => {
          "version" => "0"
        },
        "permissions"  => {
          "allowX11Clients" => "false",
          "shareTempDir" => "false",
          "shareHomeDir" => "false"
        },
        "uuid" => nil,
        "display" => {
          "isHidden" => "false"
        },
        "template" => {
          "uri" => "",
          "snapshot" => ""
        },
        "remotes" => {
          "origin" => ""
        }
    }
    
    if exist?
      buf = File.open(conffile).readlines.join('')
      parse(buf)
    end    
  end

  def exist?
    File.exist?(conffile)
  end
  
  def template_uri
    @json['template']['uri']
  end
  
  def origin
    @json['remotes']['origin']
  end
  
  def origin=(uri)
    @json['remotes']['origin'] = uri
  end
  
  def validate
    raise 'uuid cannot be empty' if @json['uuid'].nil?
  end
  
  def parse(json)
    @json = JSON.parse(json)
  end
  
  def fetch
    data = @scp.download!(@remote_path + '/options.json')
    @json = JSON.parse(data)
  end
  
  # FIXME:
  def sync
    File.open("#{mountpoint}/etc/options.json", "w") do |f|
      f.puts JSON.pretty_generate(@json)
    end
  end
  
  def to_s
    @json.pretty_inspect
  end
  
  def [](key)
    @json[key.to_s]
  end
  
  private
  
  def conffile
    @basedir + '/etc/options.json'
  end
end