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

#include <iostream>
#include <fstream>

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

	void readfile(const string& path)
	{
		string line;

		std::ifstream f(path, std::ios::in);
		if (!f.is_open()) {
			throw std::runtime_error("error opening file");
		}

		while (getline(f, line)) {
			if (line.length() > 0 && line.at(0) == '#') {
				continue;
			}

			if (line == "allowX11Clients") {
				setAllowX11Clients(true);
			} else if (line == "shareTempDir") {
				setShareTempDir(true);
			} else {
				throw std::runtime_error("unrecognized option");
			}
		}

		f.close();
	}

	void writefile(const string& path)
	{
		std::ofstream f(path, std::ios::out);
		f << "# room-options-schema-version: 0" << endl;
		if (isAllowX11Clients()) {
			f << "allowX11Clients" << endl;
		}
		if (isShareTempDir()) {
			f << "shareTempDir" << endl;
		}
		f.close();
	}

private:
	// allow X programs to run
	bool allowX11Clients = false;

	// share /tmp and /var/tmp with the main system, sadly needed for X11 and other progs
	bool shareTempDir = false;
};
