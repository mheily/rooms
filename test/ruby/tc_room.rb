#!/usr/bin/env ruby

require 'minitest/autorun'

class TestRoom < Minitest::Test
  require_relative '../../libexec/room.rb'

  @@room_name = 'tmp-room-' + $$.to_s

  def setup
    logger = Logger.new(STDOUT)
    logger.level = Logger::INFO

    @room = Room.new(@@room_name, logger)
  end

  def test_initialize
    assert_equal @@room_name, @room.name
  end
  
end
