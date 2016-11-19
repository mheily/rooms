#!/usr/bin/env ruby

require 'minitest/autorun'

class TestUriExplorer < Minitest::Test
  require_relative '../../libexec/uri_explorer.rb'

  attr_reader :tmpdir
  
  # XXX-SECURITY insecure tmpdir usage
  def setup
    @tmpdir = '/tmp/ue-test'
    Dir.mkdir tmpdir
    system "echo 'hello' > /tmp/hello"
  end
  
  def teardown
    system "rm -rf #{tmpdir}"
    system 'rm /tmp/hello'
  end
  
  def test_file_scheme
    Dir.mkdir tmpdir + '/test1'
    exp = UriExplorer.new('file://' + tmpdir + '/test1')
    common_actions(exp)
  end

  def test_sftp_scheme
    raise 'please set SFTP_URI' unless ENV['SFTP_URI']
    exp = UriExplorer.new(ENV['SFTP_URI'])
    common_actions(exp)
  end
  
  # Common actions to perform against multiple backends
  def common_actions(exp)
    exp.connect
    
    # Cleanup from any previous run
    begin
      exp.rmdir 'bar'
    rescue
    end
    begin
      exp.rm 'foo'
    rescue
    end

    assert_equal ENV['LOGNAME'], exp.user
    assert_equal [], exp.ls('')
    exp.upload "/tmp/hello", '/foo' 
    assert_equal %w(foo), exp.ls('')
    exp.rename "/foo", "/foo2"
    assert_equal %w(foo2), exp.ls('')
    exp.rename "/foo2", "/foo"
    assert_equal %w(foo), exp.ls('')
    exp.mkdir 'bar'
    assert_equal [], exp.ls('/bar')
    
    exp.download '/foo', tmpdir + '/hello2'
    assert_equal 'hello', `cat #{tmpdir + '/hello2'}`.chomp
  end
  
  def test_auth
    exp = UriExplorer.new('http://bar.com/tmp/')
    assert_equal ENV['LOGNAME'], exp.user
    
    exp = UriExplorer.new('http://foo@bar.com/tmp/')
    exp.connect
    assert_equal 'foo', exp.user
    
    assert_raises ArgumentError do
      exp = UriExplorer.new('http://foo:with_password@bar.com/tmp/')
    end
    exp.connect
    assert_equal 'foo', exp.user  
  end    
end
