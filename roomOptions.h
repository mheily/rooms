/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "namespaceImport.h"

// Options that can be controlled by the user
class RoomOptions {
public:

	void getOptions(int argc, char *argv[])
	{
		for (int i = 0; i < argc; i++) {
			if (!strcmp(argv[i], "--allow-x11")) {
				setAllowX11Clients(true);
			} else if (!strcmp(argv[i], "--share-tmpdir")) {
				setShareTempDir(true);
			}
		}

	    // KLUDGE: This is needed for now, until we can create additional xauth
	    // credentials
	    if (isAllowX11Clients()) {
	    	shareTempDir = true;
	    }
	}

	bool isAllowX11Clients() const {
		return allowX11Clients;
	}

	void setAllowX11Clients(bool allowX11Clients = false) {
		this->allowX11Clients = allowX11Clients;
	}

	bool isShareTempDir() const {
		return shareTempDir;
	}

	void setShareTempDir(bool shareTempDir = false) {
		this->shareTempDir = shareTempDir;
	}

private:
	// allow X programs to run
	bool allowX11Clients = false;

	// share /tmp and /var/tmp with the main system, sadly needed for X11 and other progs
	bool shareTempDir = false;
};
