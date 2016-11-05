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

# A data structure containing metadata for all of the tags of a room
class TagIndex
  require 'json'
  require 'pp'
  
  def initialize(json: nil)
    parse(json) if json
  end
  
  def parse(json)
    @json = JSON.parse(json)
  end
  
  # The names of all tags
  def names
    @json['tags'].map { |ent| ent['name'] }
  end
  
  def to_s
    @json['tags'].pretty_inspect
  end
  
  # Delete a tag from the index.
  # Does not actually touch the ZFS snapshot.
  def delete(name: nil, uuid: nil)
    raise ArgumentError if name && uuid
    raise ArgumentError, 'UUID not implemented' unless name
    new_tags = [@json['tags'].find { |ent| ent['name'] != name }].flatten
    @json['tags'] = new_tags
  end
end