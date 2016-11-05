#!/usr/bin/env ruby

require 'minitest/autorun'

class TestSpec < Minitest::Test
  require_relative '../../libexec/spec.rb'

  def setup
    fixture = __dir__ + '/../fixtures/tc_spec.ucl'
    @spec = Room::Spec.new.load_file(fixture)
  end

  def test_initialize
    assert_equal 'com.heily.tc_spec', @spec['label']
  end
  
  def test_build
    system "room com.heily.tc_spec destroy >/dev/null 2>&1"
    @spec.build
    system "room com.heily.tc_spec destroy" or raise 'destroy failed'
  end
end
