#!/usr/bin/env ruby
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

# A room on localhost with a optional remote associated with it
class Room
  private
  
  require "json"
  require "pp"
  require 'tempfile'
  require 'net/sftp'
  require 'shellwords'

  require_relative 'log'
  require_relative 'platform'
  require_relative 'room_options'
  require_relative 'room_uri'
  require_relative 'spec'
  require_relative 'utility'
  require_relative 'tag_index'
  require_relative 'uri_explorer'
  
  attr_reader :exp
  
  public
  
  attr_reader :name, :mountpoint, :dataset, :dataset_origin, :tags, :options
  
  include RoomUtility
  
  def initialize(name)
    raise ArgumentError unless name.is_a?(String)
    @name = name
    @user = `whoami`.chomp
    @mountpoint = "/room/#{@user}/#{name}"
    @logger = Room::Log.instance.logger
    @platform = Room::Platform.new
    @options = RoomOptions.new(@mountpoint)
    @tag_index = Room::TagIndex.new(@mountpoint)
    if File.exist? @mountpoint
      @dataset = `df -h /room/#{@user}/#{name} | tail -1 | awk '{ print \$1 }'`.chomp
      @dataset_origin = `zfs get -Hp -o value origin #{@dataset}/share`.chomp
      @options.load_file(@mountpoint + '/etc/options.json')
    end
  end
 
  def Room.build(specfile)
    spec = Room::Spec.new
    spec.load_file(specfile)
    spec.build
    room = Room.new(spec['label'])
    reindex
    return room
  end

  def index
    if @index.nil?
      @index = Room::TagIndex.new(self)
      @index.load_file(@mountpoint)
    end
    @index
  end
  
  def exist?
    Room.exist?(@name)
  end
  
  def Room.exist?(name)
    File.exist? "/room/#{`whoami`.chomp}/#{name}"
  end
  
  # DEADWOOD - should stop using this, it's moved to TagIndex
  def tags
    # Strangely, this only lists the first snapshot. If you remove '-o name', it lists them all
    #command = "zfs list -H -r -t snapshot -o name #{@dataset}/share"
    command = "ls -1 #{@mountpoint}/share/.zfs/snapshot"
    logger.debug "running: #{command}"
    `#{command}`.chomp.split(/\n/).map { |x| x.sub(/.*@/, '') }
  end
  
  def is_clone?
    @options.template_uri != ''
  end
  
  def has_tag?(name)
    tags.each do |tag|
      return true if tag == name
    end
    return false
  end
  
  def tags_json
    names = tags
    nameset = tags.map do |name|
      { 'name' => name, 'compression' => 'xz', 'format' => 'zfs' }
    end
    
    JSON.pretty_generate({
      'api' => { 'version' => 0 },
      'tags' => nameset
    })
  end
  
  def options_json
    `cat #{mountpoint}/etc/options.json`.chomp
  end
    
  def template_uri=(s)
    @json['template']['uri'] = s
    save_options  
  end
  
  def template_snapshot=(s)
    @json['template']['snapshot'] = s
    save_options  
  end
  
  def uuid=(s)
    @json['uuid'] = s
    save_options  
  end
  
  # Create a ZFS replication stream and save it at [+path+]
  def create_tags_archive(path)
    cmd = [ 'zfs', 'send', '-v' ]
    if @dataset_origin == '-'
      # This room is not a ZFS clone
      if oldest_snapshot == current_snapshot
        cmd << current_snapshot
      else
        cmd << ['-I', oldest_snapshot, current_snapshot]
      end
    else
      raise 'TODO: send incremental based on origin snapshot'
    end
    cmd << [ '>', path]
    cmd.flatten!
    cmd = cmd.join(' ') #FIXME: workaround for weird issue when array is passed
    system(cmd) or raise 'send operation failed'
  end
  
  # The most recent snapshot of the 'share' dataset
  def current_snapshot
    `zfs list -r -H -t snapshot -o name -S creation #{@dataset}/share | head -1`.chomp
  end

  # The oldest snapshot of the 'share' dataset
  def oldest_snapshot
    `zfs list -r -H -t snapshot -o name -S creation #{@dataset}/share | tail -1`.chomp
  end

  # The snapshot immediately prior to [+snapname+]
  def previous_snapshot(snapname)
    prev = nil
    tags.each do |tag|
      if tag == snapname
        return prev
      else
        prev = tag
      end
    end
    raise 'Snapshot not found'
  end
  
  # Run a script inside the room
  def run_script(buf)
    raise ArgumentError unless buf.kind_of? String
    f = Tempfile.new('room-build-script')
    f.puts buf
    f.flush
    path = '/.room-script-tmp'
    system "cat #{f.path} | room #{@name} exec -u root -- dd of=#{path} status=none" or raise 'command failed'
    system "room #{@name} exec -u root -- sh -c 'chmod 755 #{path} && #{path} && rm #{path}'" or raise 'command failed'
    f.close
  end
  
  def clone
    connect if exp.nil?
    
    if is_clone?
      logger.info "Cloning #{@name} from the local template #{template_name}"

      download_template
      args = ['room', @name, 'create', '--clone', template_name, '--tag', template_tag]
      args << '-v' if ENV['ROOM_DEBUG']
      system(*args) or raise "unable to create room"
      #TESTING: tags_copy.shift # The first tag comes from the template, not the remote room.

      template_dataset = `df -h /room/#{@user}/#{template_name} | tail -1 | awk '{ print \$1 }'`.chomp
    else
      logger.info "Creating an empty room named #{@name}"
      system('room', @name, 'create', '--empty') or raise "unable to create room"
    end
    
    fetch
  end

  # Push to origin via SSH  
  def push
    if options.origin.nil?
      logger.error "No origin URI has been defined for this room"
      exit 1
    end
       
    if tags.empty?
      logger.error "Room has no tags. You must create at least one tag before pushing."
      exit 1
    end

    connect if exp.nil?
 
      # DEADWOOD - Bad idea, lets just force fully qualified paths
  #    if uri =~ /\/~\//
  #      logger.debug "converting ~ into a real path; uri=#{uri}"
  #      remote_home = ssh.exec!('echo $HOME').chomp
  #      uri.sub!(/\/~\//, remote_home + '/')
  #      logger.debug "remote home is #{remote_home}, new URI is #{uri}"
  #    end
    
    uri = URI(options.origin)

    raise "unsupported scheme; uri=#{uri}" unless %w(sftp file).include? uri.scheme
    puts "pushing room #{name} to #{uri.to_s}"

    basedir = uri.path
    safe_basedir = Shellwords.escape(basedir)

    begin
      exp.mkdir basedir
    rescue
      # WORKAROUND: not idempotent
    end

    unless exp.ls('').include? 'tags'
      exp.mkdir('tags')
    end
    @platform.mkdir(exp, 'tags')

    logger.debug "uploading options.json"
    exp.upload(mountpoint + '/etc/options.json', "options.json")

    @tag_index.construct(self) # KLUDGE
    @tag_index.push
  end
  
  private
  
  # Fetch remote metadata
  def fetch
    path = @options.origin.path
    logger.debug "examining remote path #{path}"
    
    @tag_index.connect(exp, path)
    @tag_index.fetch
  end
  
  # Establish a connection with the @origin
  def connect
    @exp = UriExplorer.new(@options.origin)
    @tag_index.connect(exp)
  end  
end