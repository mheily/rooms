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

#include "namespaceImport.h"

#include "zfsPool.h"

// Check for any ZFS pools.
bool ZfsPool::detectZfs() {
	static int result = -1;
	if (result < 0) {
		int exit_status;
		string child_stdout;
		Shell::execute("/sbin/zpool", { "list", "-H" }, exit_status);
		if (exit_status == 0) {
			result = 1;
		} else {
			result = 0;
		}
	}
	return (result == 1);
}

string ZfsPool::getNameByPath(const string& path) {
	if (!FileUtil::checkExists(path)) {
		throw std::runtime_error("path does not exist: " + path);
	}

	int exit_status;
	string child_stdout;
	Shell::execute("/sbin/zfs", { "list", "-H", "-o", "name", "/" },
		exit_status, child_stdout);
	if (exit_status != 0 || child_stdout == "") {
		throw std::runtime_error("unable to determine pool name");
	}

	// Get the top-level pool name by removing any child datasets
	size_t pos = child_stdout.find('/');
	if (pos > 0) {
		child_stdout.erase(pos, string::npos);
	}

	string errorMsg;
	if (!ZfsPool::validateName(child_stdout, errorMsg)) {
		log_error("illegal pool name: %s", errorMsg.c_str());
		throw std::runtime_error("invalid pool name");
	}
	return child_stdout;
}
