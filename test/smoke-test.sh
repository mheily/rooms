#!/bin/sh -ex

remote_base="ssh://arise.daemonspawn.org/home/mark/rooms"

rebuild() {
	cd ..
	make clean
	make all -j4 CFLAGS="-g -O0 -static"
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
	room smoketest create --uri=file://`pwd`/base.txz
	room smoketest exec -u root -- mkdir /test2
	room smoketest exec test -d /test2
	room smoketest exec test -d a_nonexistest_file && false || true
	room smoketest destroy
}

test3() {
	uuid=`uuid -v 4`
	room smoketest create --uri=file://`pwd`/base.txz
	room smoketest tag base create -v
	room smoketest push -u ssh://arise.daemonspawn.org/~/rooms/smoketest.$uuid -v
	room smoketest pull -v
	room smoketest destroy
	
	# TODO : would like to run "room smoketest destroy --remote" instead
	ssh arise.daemonspawn.org rm -rf ~/rooms/smoketest.$uuid
}

test4() {
	room smoketest create --uri=file://`pwd`/base.txz
	room smoketest tag base create -v
	room clone smoketest smoketest2 -v
	room smoketest2 destroy -v
	room smoketest destroy -v
}

test5() {
	room smoketest create --uri=file://`pwd`/base.txz
	room smoketest tag base create -v
	room clone smoketest smoketest2 -v
	cat /room/$LOGNAME/smoketest2/etc/options.json
	room smoketest2 destroy -v
	room smoketest destroy -v
}

test6() {
	#room smoketest-base create --uri=file://`pwd`/base.txz
	#room smoketest-base tag tag1 create
	#room smoketest-base push -u $remote_base/smoketest-base
	
	room smoketest-clone destroy || true
	room smoketest-base destroy || true
	
	room clone $remote_base/smoketest-base
	room clone smoketest-base smoketest-clone
	room smoketest-clone exec -u root -- mkdir /test1
	room smoketest-clone tag tag2 create
	room smoketest-clone push -u $remote_base/smoketest-clone
	
	room clone -v $remote_base/smoketest-clone
	
	room smoketest-clone destroy || true
	room smoketest-base destroy || true
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