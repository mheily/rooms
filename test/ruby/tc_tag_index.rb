#!/usr/bin/env ruby

require 'minitest/autorun'
require_relative '../../libexec/tag_index.rb'

class TestTagIndex < Minitest::Test

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
end