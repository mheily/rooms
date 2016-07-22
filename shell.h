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
#include "logger.h"

class Shell {
public:
	static string popen_readline(const string& command);

	static int execute2(const char *path, const std::vector<std::string>& args, int& exit_status) {
		char* const envp[] = {
				(char*)"HOME=/",
				(char*)"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
				(char*)"LC_ALL=C",
				NULL
		};

		string argv_s = path;
		std::vector<char*> argv;
		argv.push_back(const_cast<char*>(path));
		for (auto it = args.begin(); it != args.end(); ++it) {
			argv.push_back(const_cast<char*>(it->c_str()));
			argv_s.append(" " + *it);
		}
		argv.push_back(NULL);

		log_debug("executing: %s", argv_s.c_str());

		pid_t pid = fork();
		if (pid < 0) {
			log_errno("fork(2)");
			throw std::runtime_error("fork failed");
		}
		if (pid == 0) {
			if (execve(path, argv.data(), envp) < 0) {
				log_errno("execve(2)");
				throw std::runtime_error("execve failed");
			}
			exit(1);
		} else {
			int status;
			if (waitpid(pid, &status, 0) < 0) {
				log_errno("waitpid(2)");
				throw std::runtime_error("waitpid failed");
			}
			if (!WIFEXITED(status)) {
				throw std::runtime_error("abnormal child termination");
			}
			exit_status = status;
			return WEXITSTATUS(status);
		}
	}

	static void execute2(const char *path, const std::vector<std::string>& args) {
		int rv = Shell::execute2(path, args, rv);
		if (rv != 0) {
			throw std::runtime_error("command returned a non-zero exit code");
		}
	}

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
