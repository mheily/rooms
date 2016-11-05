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

# A specification for how to build a room
class Room
  
  # TESTING
  class Cachefile
    require 'base64'

    attr_reader :path
    
    def initialize(domain, key)
      raise 'too long' if ((domain.length + key.length) > 500)
      
      @user = `/usr/bin/whoami`.chomp
      @tmpdir = '/room/' + @user + '/.tmp'
      @domain = domain
      @key = key
      @path = @tmpdir + '/' + @domain + '-' + encode(@key)
    end
    
    def exist?
      File.exist?(@path)
    end
    
    private
    
    def encode(key)
      Base64.encode64(key).gsub(/\n/, '').chomp
    end
  end
  
  class Spec
    require 'json'
    require 'pp'
    
    require_relative 'room'
  
    def initialize
      @json = Hash.new
    end
    
    def load_file(path)
      raise Errno::ENOENT unless File.exist?(path)
      
      @ucl = `uclcmd get --file #{path} -c ''`
      if $? != 0
        raise 'libucl parse error'
      end
      
      parse(@ucl)
      self
    end
    
    def parse(buf)
      @ucl = buf
      @json = JSON.parse(buf)
    end
    
    def [](key)
      @json[key]
    end
    
    # Build a room based on the spec
    def build
      tmpdir = "/room/" + `whoami`.chomp + '/.tmp'
      archive = Cachefile.new('room-build', @json['base']['uri'])
      unless archive.exist?
        system("fetch -o #{archive.path} #{@json['base']['uri']}") or raise 'fetch failed'
      end    

      system("sha512 -c #{@json['base']['sha512']} #{archive.path} >/dev/null") or raise 'checksum mismatch'
      system "room #{@json['label']} create #{permissions} --uri=file://#{archive.path}"
      
      room = Room.new(@json['label'])
      room.run_script @json['base']['script']
      
      system "room #{@json['label']} snapshot #{@json['base']['tag']} create"
      
      # Save the spec file
      File.open(room.mountpoint + "/etc/Roomfile", 'w') do |f|
        f.puts @ucl
      end
    end
  
    private
    
    # Return the CLI options to room that match the requested permissions
    def permissions
      perms = @json['permissions']
      return '' unless perms
      result = []
      options = { 
        'allowX11Clients' => '--allow-x11',
        'shareTempDir' => '--share-tempdir',
        'shareHomeDir' => '--share-home',
      }
      ['allowX11Clients', 'shareTempDir', 'shareHomeDir'].each do |key|
        if perms.has_key?(key.downcase) and perms[key.downcase] == true
          result << options[key]
        end
      end 
      result.join(' ')
    end
  end
end