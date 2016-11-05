#!/usr/bin/env ruby

require 'minitest/autorun'

class TestTag < Minitest::Test
  require 'json'
  require 'net/scp'
  require_relative '../../libexec/tag.rb'

  @@fixture = <<-__EOF__
      {
        "name": "base",
        "compression": "xz",
        "format": "zfs",
        "uuid": "046e5f47-ee9d-4b40-8429-131079737762"
      }
  __EOF__
  
  def setup
    @tag = Room::Tag.new(JSON.parse(@@fixture)) 
  end
  
  def test_download
    scp = Net::SCP.start("arise.daemonspawn.org", ENV['LOGNAME'])
    remote_path = '/home/mark/rooms/fbsd11'
    destdir = '/room/mark/fbsd11'
    @tag.fetch(scp: scp, remote_path: remote_path, destdir: destdir)
  end
end
