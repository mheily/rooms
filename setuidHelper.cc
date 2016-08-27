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
#include "logger.h"
#include "setuidHelper.h"

static bool isInitialized = false;
static bool isLoweredPrivs = false;
static uid_t euid = 999999999; // KLUDGE: hopefully an unused UID
static gid_t egid = 999999999; // KLUDGE: hopefully an unused GID

static bool debugModule = false;
#define debug_printf(fmt,...) do { \
	if (debugModule) { printf(fmt"\n", ## __VA_ARGS__); } \
} while (0)

static void debugPrintUid() {
	if (debugModule) {
		uid_t real, effective, saved;
		if (getresuid(&real, &effective, &saved) < 0) {
			throw std::system_error(errno, std::system_category());
		}
		printf("getresuid(2): real=%d effective=%d saved=%d\n", real, effective, saved);
	}
}

void SetuidHelper::raisePrivileges() {
	uid_t uid;
	gid_t gid;

	if (!isInitialized) {
		throw std::logic_error("must call checkPrivileges() first");
	}

	if (!isLoweredPrivs) {
		throw std::logic_error("privileges are not currently lowered");
	}

	uid = getuid();
	gid = getgid();

	debugPrintUid();

	if (setresuid(0, 0, 0) < 0) {
		throw std::system_error(errno, std::system_category());
	}
	debugPrintUid();

	if (setegid(0) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	if (geteuid() != 0) {
		throw std::runtime_error("unable to regain priviliges");
	}

	debugPrintUid();

	isLoweredPrivs = false;
}

void SetuidHelper::lowerPrivileges() {
	if (!isInitialized) {
		throw std::logic_error("must call checkPrivileges() first");
	}

	if (isLoweredPrivs) {
		throw std::logic_error("privileges already lowered");
	}

	log_debug("lowering privileges (current: uid=%d, euid=%d)", getuid(), geteuid());

	// TODO: should call getgroups(3) to save the current grouplist,
	//    and restore the privileges later
	if (setgroups(0, NULL) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	if (setegid(egid) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	if (seteuid(euid) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	debugPrintUid();

	isLoweredPrivs = true;
}

void SetuidHelper::checkPrivileges() {

	// FIXME: what about the case where euid != 0 && uid != 0 ?
	// the entire raise/lower will not work, and should throw
	// exceptions.

	debugPrintUid();

	if (geteuid() == 0 && getuid() == 0 && getenv("SUDO_UID")) {
		const char* buf = getenv("SUDO_UID");
		if (buf) {
			// FIXME: assumes gid == uid
			egid = euid = std::stoul(buf);
			//printf("2 got %d %d\n", euid, egid);
		} else {
			throw std::runtime_error("Unable to parse SUDO_UID");
		}
	} else {
		euid = getuid();
		egid = getgid();
		//printf("got %d %d\n", euid, egid);
	}

	if (setresuid(-1, -1, 0) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	isInitialized = true;
	isLoweredPrivs = false;
}

uid_t SetuidHelper::getActualUid()
{
	return euid;
}
