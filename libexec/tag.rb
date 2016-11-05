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

# A snapshot of a room
module Room
  class Tag
    require 'json'
    require 'pp'
  
    def initialize(parsed_json)
      @data = parsed_json
      @uuid = @data['uuid']
    end
  
    # @param scp [Net::SCP] a session
    # @param remote_path [String] the remote directory where the room is stored
    # @param destdir [String] the directory to download the tag to   
    def fetch(scp: nil, remote_path: nil, destdir: nil)
      raise ArgumentError unless scp && remote_path && destdir
      src = remote_path + '/tags/' + @uuid
      dst = destdir + '/tags'
      data = scp.download!(src, dst)
    end
  
  end
end