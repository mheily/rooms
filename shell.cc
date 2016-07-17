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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <locale>
#include <regex>
#include <string>
#include <streambuf>
#include <unordered_set>

extern "C" {
#include <getopt.h>
#include <jail.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>

static FILE *logfile;
#include "logger.h"
}

using std::cout;
using std::cin;
using std::endl;
using std::string;

class Shell {
public:
	static string popen_readline(const string& command);

	static int executeWithStatus(const string& command) {
		log_debug("running %s", command.c_str());
		int status = system(command.c_str());
		if (status < 0) {
			log_error("command failed: %s", command.c_str());
			throw std::runtime_error("command failed");
		}

		return WEXITSTATUS(status);
	}

	static void execute(const string& command) {
		int status = executeWithStatus(command);
		if (status != 0) {
			throw std::runtime_error("command returned unexpected status code " +
					std::to_string(status) + ": " + command);
		}
	}
};

// Run a command that is expected to return a single line of output
// Return the line, without the trailing newline
string Shell::popen_readline(const string& command)
{
	FILE *in;
	char buf[4096];

	in = popen(command.c_str(), "r");
	if (!in) {
		throw std::runtime_error("popen failed");
	}
	if (fgets(buf, sizeof(buf), in) == NULL) {
		throw std::runtime_error("empty response");
	}
	if (ferror(in)) {
		throw std::runtime_error("ferror");
	}
	if (feof(in) != 0) {
		throw std::runtime_error("buffer too small");
	}

	string s = buf;
	if (!s.empty() && s[s.length() - 1] == '\n') {
		s.erase(s.length() - 1);
	}

	return s;
}
