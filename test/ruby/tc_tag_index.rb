#!/usr/bin/env ruby

require 'minitest/autorun'

class TestTagIndex < Minitest::Test
  require_relative '../../libexec/tag_index.rb'
  require_relative '../../libexec/room.rb'
  require_relative 'micro_room'

  @@fixture = <<-__EOF__
  {
    "api": {
      "version": 0
    },
    "tags": [
      {
        "name": "base",
        "compression": "xz",
        "format": "zfs"
      },
      {
        "name": "tag1",
        "compression": "xz",
        "format": "zfs"
      }
    ]
  }
  __EOF__
  
  def setup
    @index = TagIndex.new(json: @@fixture) 
  end
  
  def test_names
    assert_equal %w(base tag1), @index.names
  end
  
  def test_to_s
    assert @index.to_s
  end
  
  def test_delete
    @index.delete(name: 'tag1')
    assert_equal %w(base), @index.names
  end
  
  def test_construct
    label = 'test.construct.' + $$.to_s
    room = Room.build(MicroRoom.new(label).specfile)
    @index = TagIndex.new
    @index.construct(room)
    raise 'hi'
  end
end

class TestRemoteTagIndex < Minitest::Test
  require 'net/scp'
  
  def test_remote_fetch
    scp = Net::SCP.start("arise.daemonspawn.org", ENV['LOGNAME'])
    index = TagIndex.new(scp: scp, remote_path: '/home/mark/rooms/fbsd11') 
    assert_equal %w(base), index.names
  end
end