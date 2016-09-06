#!/bin/sh 
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

ROOM="sudo ./room"

err() {
	echo "*** ERROR *** $*"
	exit 1
}

cleanup() {
	for name in `$ROOM list | grep -v FreeBSD` ; do
		echo "halting $room"
		$ROOM $name halt || err "halt failed"
		echo "destroying $room"
		$ROOM $name destroy || err "destroy failed"
	done
	
	$ROOM FreeBSD-10.3 destroy
	sudo zfs destroy tank/room/`whoami`
	sudo zfs destroy tank/room
	sudo rmdir /room
	test -d /room && err "cleanup failed"
}

create() {
	set -ex
	test -f /tmp/base.txz || fetch -o /tmp ftp://ftp.freebsd.org/pub/FreeBSD/releases/amd64/10.3-RELEASE/base.txz
	echo tank | $ROOM init
	$ROOM FreeBSD-10.3 install --uri /tmp/base.txz	
	set +ex
}

test_clone() {
	set -ex
	$ROOM clone FreeBSD-10.3 test123
	set +ex
}

test_pkg_install() {
	set -ex
	$ROOM test123 exec -u root -- /usr/bin/id | grep root
	$ROOM test123 exec -u root -- /usr/sbin/pkg install -y xeyes
	set +ex
}

echo "*** WARNING *** This will destroy all rooms on the system"
echo "Type 'destroy' to confirm this is OK"
read response
if [ "$response" != "destroy" ] ; then exit 1 ; fi

if [ $1 = "cleanup" ] ; then
	cleanup
	exit 0
fi

create
test_clone
test_pkg_install
cleanup
