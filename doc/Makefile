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

XMLTO=xmlto --skip-validation

all: clean check build-html build-html-nochunks
	
check:
	jing docbookxi.rng *.xml || true # FIXME: ../../src/*/*.xml

build-html:
	$(XMLTO) -o html html book.xml

build-html-nochunks:
	$(XMLTO) -o html-nochunks html-nochunks book.xml

preview-nochunks: clean build-html-nochunks
	firefox --new-window ./html-nochunks/book.html

preview: clean build-html
	firefox --new-window ./html/index.html

publish:
	rm -f ../docs/*.html
	cp html/*.html ../docs/
	cd ../docs && git commit -m'Regenerate GitHub pages HTML' *.html

clean:
	rm -rf html html-nochunks
	mkdir html html-nochunks

.PHONY: all preview publish clean
