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

extern "C" {
#ifdef __linux__
#include <mntent.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#include <stdio.h>
}

#include "fileUtil.h"

class MountUtil {
public:

	static bool checkIsMounted(const std::string& path) {
#ifdef __linux__
		FILE* f;
		struct mntent* ent;

		auto mnttab = std::string("/etc/mtab");
		f = setmntent(mnttab.c_str(), "r");
		if (!f) {
			err(1, "setmntent(3) of %s", mnttab.c_str());
		}
		while ((ent = getmntent(f)) != NULL) {
			if (string(ent->mnt_dir) == path) {
				return true;
			}
		}
		endmntent(f);
		return false;
#elif defined(__FreeBSD__)
		struct statfs *sfs;
		int size;

		if ((size = getmntinfo(&sfs, MNT_NOWAIT)) == 0) {
			err(1, "getmntinfo(3)");
		}
		for (int i = 0; i < size; i++) {
			//std::cout << path << " " << sfs[i].f_mntonname << "\n";
			if (!strcmp(path.c_str(), sfs[i].f_mntonname)) {
				return true;
			}
		}
		return false;
#else
		errx(1, "FIXME: checkIsMounted not implemented yet");
#endif
	}

	static void recursiveUnmount(const std::string& chrootDir) {
#ifdef __linux__
	FILE* f;
	struct mntent* ent;

	if (chrootDir == "") {
		errx(1, "empty chrootdir");
	}

	auto query = chrootDir + "/";

	//auto mnttab = std::string(chrootDir + "/proc/mounts");
	auto mnttab = std::string("/etc/mtab");
	if (!FileUtil::checkExists(mnttab)) {
		log_warning("no mounttab available");
		return;
	}
	f = setmntent(mnttab.c_str(), "r");
	if (!f) {
		err(1, "setmntent(3) of %s", mnttab.c_str());
	}
	while ((ent = getmntent(f)) != NULL) {
		if (string(ent->mnt_dir).compare(0, query.length(), query) == 0) {
		//std::cout << ent->mnt_fsname << " -- " << ent->mnt_dir << "\n";
		log_debug("unmounting %s", ent->mnt_dir);
SetuidHelper::raisePrivileges();
		FileUtil::unmount(ent->mnt_dir, MNT_FORCE);
SetuidHelper::lowerPrivileges();
		}
	}
	endmntent(f);
#else
		errx(1, "FIXME: recursive umount not implemented yet");
		// on FreeBSD: mount -p | awk '{print $2}' | egrep '^/foo/'
#endif
	}
};
