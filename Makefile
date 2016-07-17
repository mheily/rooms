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

SOURCES=main.cc room.cc shell.cc
HEADERS=shell.h namespaceImport.h logger.h fileUtil.h

all: room

room: $(SOURCES) $(HEADERS)
	$(CXX) -std=c++11 -isystem /usr/local/include -o room $(SOURCES)

install: room
	install -m 6755 -o root -g wheel room /usr/local/bin/room

check: check-setup check-create check-destroy

clean:
	rm room

check-setup:
	make
	sudo make install

check-create: check-setup
	room create foo

check-destroy:
	room destroy foo
	
.PHONY: check check-setup check-create check-destroy