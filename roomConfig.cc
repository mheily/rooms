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

#include "logger.h"
#include "namespaceImport.h"
#include "roomConfig.h"
#include "passwdEntry.h"
#include "shell.h"
#include "zfsPool.h"

// Check for any ZFS pools.
static bool detectZfs() {
	int exit_status;
	string child_stdout;
	Shell::execute("/bin/sh", { "-c", "zpool list -H" }, exit_status);
	if (exit_status == 0) {
		return true;
	} else {
		return false;
	}
}

RoomConfig::RoomConfig() {
//	if (geteuid() != 0) {
//		throw std::runtime_error(
//				"Insufficient permissions; must be run as root");
//	}

	uid_t real_uid = getuid();
	if (getuid() == geteuid()) {
		const char* buf = getenv("SUDO_UID");
		if (buf) {
			real_uid = std::stoul(buf);
		}
//		else {
//			throw std::runtime_error(
//					"The root user is not allowed to create rooms. Use a normal user account instead");
//		}
	}
	setOwnerUid(real_uid);
	log_debug("uid=%d euid=%d real_uid=%d", getuid(), geteuid(), real_uid);

	PasswdEntry pwent(real_uid);
	setOwnerLogin(pwent.getLogin());

	b_useZfs = detectZfs();
	if (useZfs()) {
		zpoolName = ZfsPool::getNameByPath(roomDir);
		setParentDataset(zpoolName + "/room/" + ownerLogin);
	}
}
