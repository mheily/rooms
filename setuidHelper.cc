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

#ifdef __linux__
// Required to get setgroups(2)
#define _BSD_SOURCE
#endif

#include <err.h>
#include <sys/stat.h>
#include <grp.h>


#include "namespaceImport.h"
#include "logger.h"
#include "setuidHelper.h"

static bool isInitialized = false;
static bool isLoweredPrivs = false;
static bool isDroppedPrivs = false;
static uid_t euid = 999999999; // KLUDGE: hopefully an unused UID
static gid_t egid = 999999999; // KLUDGE: hopefully an unused GID
static mode_t saved_umask = S_IWGRP|S_IWOTH;

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
		printf("priv state: lowered=%d dropped=%d\n", (int)isLoweredPrivs, (int)isDroppedPrivs);
	}
}

void SetuidHelper::raisePrivileges() {
	debugPrintUid();

	if (isDroppedPrivs) {
		errx(1, "privileges are dropped");
	}

	if (!isInitialized) {
		errx(1, "must call checkPrivileges() first");
	}

	if (!isLoweredPrivs) {
		errx(1, "privileges are not currently lowered");
	}

	saved_umask = umask(S_IWGRP|S_IWOTH);

	if (debugModule) {
		log_debug("raising privileges");
	}

	if (setresuid(0, 0, 0) < 0) {
		err(1, "setresuid(2)");
	}

	if (setresgid(0, 0, 0) < 0) {
		err(1, "setresgid(2)");
	}

	if (geteuid() != 0) {
		errx(1, "unable to regain priviliges");
	}

	debugPrintUid();

	isLoweredPrivs = false;
}

void SetuidHelper::lowerPrivileges() {
	debugPrintUid();

	if (!isInitialized) {
		throw std::logic_error("must call checkPrivileges() first");
	}

	if (isDroppedPrivs) {
		throw std::logic_error("privileges are dropped");
	}

	if (isLoweredPrivs) {
		errx(1, "privileges already lowered");
	}

	if (debugModule) {
		log_debug("lowering privileges (current: uid=%d, euid=%d)", getuid(), geteuid());
	}

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

	(void) umask(saved_umask);


	isLoweredPrivs = true;
}

void SetuidHelper::dropPrivileges() {
	if (!isInitialized) {
		throw std::logic_error("must call checkPrivileges() first");
	}

	if (isDroppedPrivs) {
		throw std::logic_error("privileges are already dropped");
	}

	if (isLoweredPrivs) {
		raisePrivileges();
	}

	log_debug("dropping privileges (current: uid=%d, euid=%d)", getuid(), geteuid());

	if (setgroups(0, NULL) < 0) {
		log_errno("setgroups(2)");
		throw std::system_error(errno, std::system_category());
	}

	if (setresgid(egid, egid, egid) < 0) {
		log_errno("setresgid(2)");
		throw std::system_error(errno, std::system_category());
	}

	if (setresuid(euid, euid, euid) < 0) {
		log_errno("setresuid(2)");
		throw std::system_error(errno, std::system_category());
	}

	debugPrintUid();
	isLoweredPrivs = false;
	isDroppedPrivs = true;
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
			//printf("2 got %d %d %d\n", euid, egid, getegid());
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

void SetuidHelper::logPrivileges()
{
	bool saved = debugModule;
	debugModule = true;
	debugPrintUid();
	debugModule = saved;
}

