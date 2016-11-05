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
   
  # @param scp [Net::SCP] open connection to the remote server
  # @param remote_path [String] path to the remote room
  def initialize(json: nil, scp: nil, remote_path: nil)
    parse(json) if json
    @scp = scp
    @remote_path = remote_path
    fetch if @scp and @remote_path
  end
  
  def parse(json)
    @json = JSON.parse(json)
  end
  
  def fetch
    data = @scp.download!(@remote_path + '/options.json')
    @json = JSON.parse(data)
  end
   
  def to_s
    @json.pretty_inspect
  end
  
  def [](key)
    @json[key.to_s]
  end
end