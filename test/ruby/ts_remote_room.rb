#!/usr/bin/env ruby

require 'minitest/autorun'
require_relative '../../libexec/remote_room.rb'

class TestRemoteRoom < Minitest::Test

  def setup
    logger = Logger.new(STDOUT)
    logger.level = Logger::INFO

    @room = RemoteRoom.new(uri: 'ssh://arise.daemonspawn.org/~/rooms/fbsd11', 
      logger: logger, local_name: 'myroom', tmpdir: '/tmp/nonexistent')
  end

  def test_initialize
    assert_equal 'fbsd11', @room.name
  end
  
  def test_connect
    @room.connect
  end

  def FIXME_test_clone
    @room.connect
    @room.fetch
    @room.clone('test1234')
  end  
end