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

# Given a URI, provide methods for downloading, uploading, and listing
# directory entries. For HTTP/HTTPS, the server must offer JSON
# directory indexes.
class UriExplorer
  require 'net/sftp'
  require 'net/http'
  require 'uri'
  
  attr_reader :user, :uri
  
  def initialize(uri)
    if uri.is_a? String
      @uri = URI(uri)
    elsif uri.is_a? URI
      @uri = uri
    else
      raise TypeError, 'invalid URI'
    end
    @user = @uri.userinfo || ENV['LOGNAME']
    if @user =~ /\:/
      raise ArgumentError, "unsupported URI: embedded passwords are not allowed"
    end
  end
  
  def connect
    case @uri.scheme
    when 'http', 'https'
      @http = Net::HTTP.new(uri.host, uri.port)
    when 'sftp'
      @sftp = Net::SFTP.start(uri.host, user)
    when 'file'
      return
    else
      raise 'Invalid scheme'
    end
  end
  
  def disconnect
    case @uri.scheme
    when 'http', 'https'
      @http.finish
    when 'sftp'
      @sftp.shutdown!
    when 'file'
      return
    else
      raise 'Invalid scheme'
    end
  end
  
  def ls(subdir)
    path = uri.path + '/' + subdir
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      entries = @sftp.dir.entries(path).map { |e| e.name }
    when 'file'
      entries = Dir.entries(path)
    else
      raise 'Invalid scheme'
    end
    entries.select { |e| e != '.' && e != '..' }.sort
  end
  
  def mkdir(subdir)
    path = uri.path + '/' + subdir
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.mkdir! path
    when 'file'
      Dir.mkdir path
    else
      raise 'Invalid scheme'
    end
  end

  def rename(src, dst)
    src_path = uri.path + '/' + src
    dst_path = uri.path + '/' + dst
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.rename! src_path, dst_path
    when 'file'
      FileUtils.mv src_path, dst_path
    else
      raise 'Invalid scheme'
    end
  end
  
  
  def rmdir(subdir)
    path = uri.path + '/' + subdir
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.rmdir! path
    when 'file'
      Dir.rmdir path
    else
      raise 'Invalid scheme'
    end
  end

  def rm(subdir)
    path = uri.path + '/' + subdir
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.remove! path
    when 'file'
      File.unlink path
    else
      raise 'Invalid scheme'
    end
  end
      
  def upload(src, dst)
    dst_path = uri.path + '/' + dst
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.upload! src, dst_path
    when 'file'
      FileUtils.cp src, dst_path
    else
      raise 'Invalid scheme'
    end
  end
  
  def download(src, dst)
    src_path = uri.path + '/' + src
    case @uri.scheme
    when 'http', 'https'
      raise 'todo'
    when 'sftp'
      @sftp.download! src_path, dst
    when 'file'
      FileUtils.cp src_path, dst
    else
      raise 'Invalid scheme'
    end
  end
end