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

# Utility functions for working with rooms
module RoomUtility
  def setup_logger
    @logger = Logger.new(STDOUT)
    if ENV['ROOM_DEBUG']
      @logger.level = Logger::DEBUG
    else
      @logger.level = Logger::INFO
    end
  end
  
  def setup_tmpdir
    @tmpdir = Dir.mktmpdir($PROGRAM_NAME.sub(/.*\//, ''))
    at_exit { system "rm -rf #{@tmpdir}" if File.exist?(@tmpdir) }
  end
    
  def logger
    @logger
  end
  
  def system(*args)
    logger.debug 'running: ' + args.join(' ')
    Kernel.system(*args)
  end
  
end
