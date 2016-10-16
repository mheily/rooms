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

#include "shell.h"

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

string Shell::readline(int pd) {
	FILE *in = fdopen(pd, "r");
	char buf[4096];

	if (fgets(buf, sizeof(buf), in) == NULL) {
		string s = "";
		return s;
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


void Subprocess::execute(const char *path, const std::vector<std::string>& args)
{
	char* sanitized_envp[] = {
			(char*)"HOME=/",
			(char*)"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
			(char*)"LANG=C",
			(char*)"LC_ALL=C",
			(char*)"TERM=vt220",
			(char*)"LOGNAME=root",
			(char*)"USER=root",
			(char*)"SHELL=/bin/sh",
			NULL
	};

	extern char **environ;
	char** envp;
	if (preserveEnvironment) {
		envp = environ;
	} else {
		envp = (char **)sanitized_envp;
	}

	string argv_s = path;
	std::vector<char*> argv;
	argv.push_back(const_cast<char*>(path));
	for (auto it = args.begin(); it != args.end(); ++it) {
		argv.push_back(const_cast<char*>(it->c_str()));
		argv_s.append(" " + *it);
	}
	argv.push_back(NULL);

	log_debug("executing: %s", argv_s.c_str());

	int pd[2];
	if (captureStdio && pipe(pd) < 0) {
		log_errno("fork(2)");
		throw std::runtime_error("fork failed");
	}

	pid = fork();
	if (pid < 0) {
		log_errno("fork(2)");
		throw std::runtime_error("fork failed");
	}
	if (pid == 0) {
		if (captureStdio) {
			/* Connect the child process STDOUT to the parent via a pipe */
			if (dup2(pd[1], STDOUT_FILENO) < 0) {
				log_errno("dup2(2)");
				throw std::runtime_error("dup2 failed");
			}
			close(pd[1]);
			close(pd[0]);
		}
		if (dropPrivileges) {
			gid_t egid = getegid();
	        if (setresgid(egid, egid, egid) < 0) {
	                throw std::system_error(errno, std::system_category());
	        }

	        uid_t euid = geteuid();
	        if (setresuid(euid, euid, euid) < 0) {
	                throw std::system_error(errno, std::system_category());
	        }
		}
		if (::execve(path, argv.data(), envp) < 0) {
			log_errno("execve(2)");
			throw std::runtime_error("execve failed");
		}
		exit(1);
	} else {
		if (captureStdio) {
			close(pd[1]);
			child_stdout = pd[0];
		}
	}
}

int Subprocess::waitForExit() {
	int status;
	if (waitpid(pid, &status, 0) < 0) {
		savedErrno = errno;
		savedErrnoMessage = "waitpid(2)";
		return -1;
	}
	if (!WIFEXITED(status)) {
		savedErrno = 0;
		savedErrnoMessage = "!WIFEXITED";
		return -1;
	}
	exitStatus = WEXITSTATUS(status);

	return exitStatus;
}

void Subprocess::execve(const char* path, const std::vector<std::string>& args)
{
	// Convert from string to char*
	std::vector<char*> argv;
	argv.push_back(const_cast<char*>(path));
	for (auto it = args.begin(); it != args.end(); ++it) {
		argv.push_back(const_cast<char*>(it->c_str()));
	}
	argv.push_back(NULL);

	char* const envp[] = {
			(char*)"HOME=/",
			(char*)"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
			(char*)"LANG=C",
			(char*)"LC_ALL=C",
			(char*)"TERM=vt220",
			(char*)"LOGNAME=root",
			(char*)"USER=root",
			(char*)"SHELL=/bin/sh",
			NULL
	};

	if (::execve(path, argv.data(), envp) < 0) {
		log_errno("execve(2)");
		throw std::runtime_error("execve failed");
	}
}
