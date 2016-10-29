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

if [ -n "$1" ] ; then
	room smoketest destroy || true # in case a test failed
	sudo make -C .. install
	eval $1
else
	fetch_base
	rebuild
	test1
	test2
fi