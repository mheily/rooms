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
#include <pwd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
}

#include "namespaceImport.h"

extern FILE *logfile;
#include "logger.h"

class FileUtil {
public:
	static bool checkExists(const string& path) {
		if (eaccess(path.c_str(), F_OK) == 0) {
			return true;
		} else {
			if (errno != ENOENT) {
				log_errno("eaccess(2) of `%s'", path.c_str());
				throw std::system_error(errno, std::system_category());
			}
			return false;
		}
	}

	static void chmod(const string& path, mode_t mode) {
		if (::chmod(path.c_str(), mode) != 0) {
			log_errno("chmod(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void chgrp(const string& path, gid_t gid) {
		if (::chown(path.c_str(), -1, gid) != 0) {
			log_errno("chgrp(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void mkdir(const string& path, mode_t mode) {
		if (::mkdir(path.c_str(), mode) != 0) {
			log_errno("mkdir(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void rmdir(const string& path) {
		if (::rmdir(path.c_str()) != 0) {
			log_errno("rmdir(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void unlink(const string& path) {
		if (::unlink(path.c_str()) != 0) {
			log_errno("unlink(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void mkdir_idempotent(const string& path, mode_t mode, uid_t uid, gid_t gid) {
		if (::mkdir(path.c_str(), mode) != 0) {
			if (errno == EEXIST) {
				return;
			}
			log_errno("mkdir(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}

		if (chown(path.c_str(), uid, gid) != 0) {
			(void) unlink(path.c_str());
			log_errno("chown(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}

	static void unmount(const string& path, int flags) {
#ifdef __linux__
		if (::umount2(path.c_str(), flags) < 0) {
#else
		if (::unmount(path.c_str(), flags) < 0) {
#endif
			log_errno("unmount(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}
};
