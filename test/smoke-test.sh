#!/bin/sh -ex

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

test_zfs_clones() {
	dataset=tank/zfstest
	sudo zfs destroy -R $dataset || true
	sudo zfs destroy -R ${dataset}_clone || true
	sudo zfs destroy -R ${dataset}_clone2 || true
	sudo zfs create -o mountpoint=/zfstest $dataset
	sudo touch /zfstest/base
	sudo zfs snapshot $dataset@base
	sudo zfs clone -o mountpoint=/zfstest.2 $dataset@base ${dataset}_clone
	sudo zfs snapshot ${dataset}_clone@base
	sudo touch /zfstest.2/snap1
	sudo zfs snapshot ${dataset}_clone@snap1
	sudo zfs clone -o mountpoint=/zfstest.3 ${dataset}@base ${dataset}_clone2
	sudo zfs snapshot ${dataset}_clone2@base
	zfs list | grep zfstest
	zfs list -t snapshot | grep zfstest
	## ALternate solution: receive snap1 into the template, clone to a new dataset, destroy template@snap1
	## TEST THIS
	    	sudo zfs destroy -R ${dataset}_clone2 || true  #undo previous
	    	sudo zfs create ${dataset}_clone2
	
	sudo zfs send -i base ${dataset}_clone@snap1 > /tmp/snap1
	sudo zfs destroy ${dataset}_clone@snap1 # pretend it only existed in the remote
	sudo zfs recv -o origin=${dataset}_clone -F ${dataset}_clone2@snap1 < /tmp/snap1
	sudo zfs clone ${dataset}_clone@snap1 ${dataset}_clone2
	#sudo zfs promote ${dataset}_clone2
	#sudo zfs destroy ${dataset}_clone2@snap1
	#sudo zfs promote ${dataset}_clone
	echo 'wow'
	false
	## PROBLEM STARTS HERE
	sudo zfs promote ${dataset}_clone2
	sudo zfs send -P -i base ${dataset}_clone@snap1 | sudo zfs recv -F -o origin=${dataset}_clone ${dataset}_clone2@snap1
	sudo zfs promote ${dataset}_clone
	sudo zfs destroy -R $dataset
	sudo zfs destroy -R ${dataset}_clone || true
	sudo zfs destroy -R ${dataset}_clone2 || true
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
fi