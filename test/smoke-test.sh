#!/bin/sh -ex

remote_host="arise.daemonspawn.org"
remote_base="ssh://${remote_host}/home/mark/rooms"
if [ $(uname) = "FreeBSD" ] ; then
	base_txz="$(pwd)/fixtures/FreeBSD-11.0-micro-jailroot.tar.xz"
else
	base_txz="$(pwd)/fixtures/linux.tgz"
	if [ ! -e $base_txz ] ; then
  		wget -O $base_txz \
			https://mirrors.kernel.org/archlinux/iso/2016.09.03/archlinux-bootstrap-2016.09.03-x86_64.tar.gz
		# Remove the leading root.x86_64/ directory in the tarball
		tar zxf $base_txz
		tar -C root.x86_64 -zPpcf $base_txz .
	fi
fi

rebuild() {
	cd ..
	make clean
	make all -j4 CFLAGS="-g -O0 -static"
	sudo make install
	cd test
}

quick_rebuild() {
	cd ..
	make
	sudo make install
	cd test
}

fetch_base() {
	test -e base.txz || fetch ftp://ftp.freebsd.org/pub/FreeBSD/releases/amd64/amd64/11.0-RELEASE/base.txz
}

test1() {
	room list | grep -q smoketest && sudo -E ./room smoketest destroy || true

	room smoketest create
	room smoketest exec hostname
	room smoketest destroy
}

test2() {
	room smoketest create --archive=`pwd`/base.txz
	room smoketest exec -u root -- mkdir /test2
	room smoketest exec test -d /test2
	room smoketest exec test -d a_nonexistest_file && false || true
	room smoketest destroy
}

test3() {
	uuid=`uuid -v 4`
	room smoketest create --archive=`pwd`/base.txz
	room smoketest tag base create -v
	room smoketest push -u ssh://arise.daemonspawn.org/~/rooms/smoketest.$uuid -v
	room smoketest pull -v
	room smoketest destroy
	false
	# TODO : would like to run "room smoketest destroy --remote" instead
	ssh arise.daemonspawn.org rm -rf ~/rooms/smoketest.$uuid
}

test4() {
	room smoketest create --archive=`pwd`/base.txz
	room smoketest tag base create -v
	room clone smoketest smoketest2 -v
	room smoketest2 destroy -v
	room smoketest destroy -v
}

test5() {
	room smoketest create --archive=`pwd`/base.txz
	room smoketest tag base create -v
	room clone smoketest smoketest2 -v
	cat /room/$LOGNAME/smoketest2/etc/options.json
	room smoketest2 destroy -v
	room smoketest destroy -v
}

test6_clone() {
	echo 'testing room-clone of remote'
	room clone -v file:///tmp/smoketest-base
}

test6() {
	room list | grep -q smoketest-base || {
		room smoketest-base create --archive=$base_txz
		room smoketest-base tag tag1 create
	}
	
	echo 'testing room-push'
	rm -rf /tmp/smoketest-base
	room smoketest-base push -v -u file:///tmp/smoketest-base
	room smoketest-base destroy
	test6_clone
		
	echo 'FIXME--INCOMPLETE';exit 1
	room smoketest-clone destroy || true
	room smoketest-base destroy || true
	
	room clone -v $remote_base/smoketest-base
	room clone -v smoketest-base smoketest-clone
	room smoketest-clone exec -u root -- mkdir /test1
	room smoketest-clone tag tag2 create
	room smoketest-clone push -u $remote_base/smoketest-clone
	
	room clone -v $remote_base/smoketest-clone
	
	room smoketest-clone destroy || true
	room smoketest-base destroy || true
}

test_localhost_push() {
	room='localtest'
	
	room list | grep -q $room || {
		room $room create -v --archive=$base_txz
		room $room tag tag1 create
	}
	
	echo 'testing room-push'
	rm -rf /tmp/$room
	
	mkdir /tmp/$room
	room $room push -v -u file:///tmp/$room
	echo 'Room after pushing:'
	find /tmp/$room
	
	room $room destroy
	room clone file:///tmp/$room
	room $room destroy
	rm -rf /tmp/$room
}

test_fbsd11_push() {
	room fbsd11 push -u $remote_base/fbsd11 -v
	room fbsd11 pull -v
}

test_zfs_clones() {
	dataset=tank/zfstest
	
	# Cleanup any half-broken stuff from last time
	sudo zfs destroy -R $dataset || true
	sudo zfs destroy -R ${dataset}_clone || true
	
	# Create the freebsd-11 template
	sudo zfs create -o mountpoint=/zfstest $dataset
	sudo touch /zfstest/base
	sudo zfs snapshot $dataset@base
	#sudo zfs bookmark $dataset@base $dataset#base
	
	# Create a shallow clone of the template
	sudo zfs clone -o mountpoint=/zfstest.2 $dataset@base ${dataset}_clone
	
	# Make changes to the clone, and create a new tag
	sudo touch /zfstest.2/snap1
	sudo zfs snapshot ${dataset}_clone@snap1
	
	# Push the clone to origin
	sudo zfs send -v -I ${dataset}@base ${dataset}_clone@snap1 > /tmp/clone1.snap1
	sudo zfs destroy -r ${dataset}_clone
	
	# Reconstruct the clone
	#sudo zfs clone -o mountpoint=/zfstest.2 $dataset@base ${dataset}_clone
	#sudo zfs snapshot ${dataset}_clone@base
	sudo zfs recv -v -o origin="tank/zfstest@base" -F ${dataset}_clone < /tmp/clone1.snap1
	sudo zfs set mountpoint=/zfstest.2 ${dataset}_clone
	test -e /zfstest.2/snap1
	
	# Cleanup
	sudo zfs destroy -R $dataset
	sudo zfs destroy -R ${dataset}_clone || true
	sudo rm /tmp/clone1.snap1
}

if [ -n "$1" ] ; then
	room smoketest destroy || true # in case a test failed
	sudo make -C .. install
	eval $1
else
	fetch_base
	rebuild
	test1
	test2
	test3
	test4
	test5
	test6
	#test_zfs_clones
fi

echo "SUCCESS: No errors detected"
