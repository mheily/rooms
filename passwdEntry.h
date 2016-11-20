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

#include <pwd.h>

#include "namespaceImport.h"
#include "logger.h"

class PasswdEntry {
public:
	PasswdEntry(uid_t uid) {
		struct passwd *result;

		if (getpwuid_r(uid, &pwent, (char*)&pwent_buf,
			sizeof(pwent_buf), &result) != 0) {
			log_errno("getpwuid_r(3)");
			throw std::system_error(errno, std::system_category());
		}
		if (result == NULL) {
			throw std::runtime_error("missing passwd entry for UID " + std::to_string(uid));
		}
	}

	// get the "real" UID of the current process,
	// taking into account sudo(1) and running setuid binaries
	static uid_t getRealUid() {
		uid_t real_uid = getuid();
		if ((0 == getuid()) == geteuid()) {
			const char* buf = getenv("SUDO_UID");
			if (buf) {
				real_uid = std::stoul(buf);
			}
		}
		//log_debug("uid=%d euid=%d real_uid=%d", getuid(), geteuid(), real_uid);
		return real_uid;
	}

	const char* getLogin() const {
		return pwent.pw_name;
	}

	const char* getGecos() const {
		return pwent.pw_gecos;
	}

	const char* getHome() const {
		return pwent.pw_dir;
	}

	const char* getShell() const {
		return pwent.pw_shell;
	}

private:
	struct passwd pwent;
	char pwent_buf[9999]; // storage used by getpwuid_r(3)
};
