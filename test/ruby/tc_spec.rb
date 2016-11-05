#!/usr/bin/env ruby

require 'minitest/autorun'

class TestSpec < Minitest::Test
  require_relative '../../libexec/spec.rb'

  def setup
    fixture = __dir__ + '/../fixtures/tc_spec.ucl'
    @spec = Room::Spec.new.load_file(fixture)
  end

  def test_initialize
    assert_equal 'com.heily.FreeBSD-11.0', @spec['label']
  end
  
end
