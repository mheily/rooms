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

class MicroRoom
  require 'tempfile'
  require 'erb'
  
  attr_reader :specfile
  
  def initialize(label)
    @label = label
    @fixturedir = __dir__.sub(/\/ruby\z/, '/fixtures')
    ucl_tempfile = Tempfile.new('micro-room')
    template = `cat #{__dir__}/../fixtures/micro-room.ucl.erb`
    renderer = ERB.new(template)
    result = renderer.result(binding)
    ucl_tempfile.write(result)
    ucl_tempfile.flush
    @specfile = ucl_tempfile.path
  end
    
end
