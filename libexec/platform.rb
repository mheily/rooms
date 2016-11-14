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
class Room
  # Detect the current platform and return a unique platform ID
  class Platform
    attr_accessor :architecture, :logger
    
    def initialize
      @architecture = detect_architecture
      @kernel = detect_kernel
      @abi_version = detect_abi_version
      @logger = Room::Log.instance.logger
    end
    
    def to_s
      [@architecture, @kernel, @abi_version].join('/')
    end
    
    # Given an active SFTP connection and the path to the tags/ directory
    # of a room, create the directories for the current platform
    def mkdir(sftp, tagdir)
      logger.debug("examining #{tagdir}")
      # Architecture
      dirent = sftp.dir.entries(tagdir).map { |e| e.name }
      unless dirent.include?(@architecture)
        dir = tagdir + '/' + @architecture
        logger.debug "creating #{dir}"
        sftp.mkdir!(dir)
      end
      
      # Kernel
      dirent = sftp.dir.entries(tagdir + '/' + @architecture).map { |e| e.name }
      unless dirent.include?(@kernel)
        dir = tagdir + '/' + @architecture + '/' + @kernel
        logger.debug "creating #{dir}"
        sftp.mkdir!(dir)
      end

      # ABI version
      dirent = sftp.dir.entries(tagdir + '/' + @architecture + '/' + @kernel).map { |e| e.name }
      unless dirent.include?(@abi_version)
        dir = tagdir + '/' + @architecture + '/' + @kernel + '/' + @abi_version
        logger.debug "creating #{dir}"
        sftp.mkdir!(dir)
      end
    end
    
    private
    
    def detect_architecture
      `uname -m`.chomp
    end
    
    def detect_kernel
      `uname -s`.chomp
    end
    
    def detect_abi_version
      # KLUDGE: return the major version of FreeBSD
      `uname -r`.chomp.sub(/\..*/, '')
    end
  end
end