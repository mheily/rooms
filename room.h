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
}

#include "namespaceImport.h"
#include "shell.h"
#include "fileUtil.h"

extern FILE *logfile;
#include "logger.h"

class Room {
public:
	Room(const string& managerRoomDir, const string& name, uid_t uid);

	void create(const string& baseTarball);
	void killAllProcesses();
	void destroy();
	void enter();

private:
	char ownerPwEntBuf[9999]; // storage used by getpwuid_r(3)
	struct passwd ownerPwEnt; // the owner's passwd(5) entry
	uid_t  ownerUid;  // the UID who owns the room
	string roomDir;   // copy of RoomManager::roomDir
	string chrootDir; // path to the root of the chroot environment
	string roomName;  // name of this room
	string jailName;  // name of the jail
	bool allowX11Clients = true; // allow X programs to run
	bool shareTempDir = true; // share /tmp with the main system, sadly needed for X11 and other progs

	void getPasswdInfo(uid_t uid);
	bool jailExists();
	void customizeWithoutRoot();
	void enableX11Clients();
	void validateName(const string& name);
};
