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

#if 0
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
#include "shell.h"
#include "fileUtil.h"
#include "room.h"

#endif

#include "namespaceImport.h"

extern FILE *logfile;
#include "logger.h"

class RoomManager {
public:
	void bootstrap();
	static bool isBootstrapComplete();
	void setup(uid_t);
	void createRoom(const string& name);
	void destroyRoom(const string& name);
	Room getRoomByName(const string& name);
	void listRooms();

private:
	uid_t ownerUid;
	string ownerLogin;
	string baseTarball = "/var/cache/room-base.txz";
	string baseUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/11.0-BETA1/base.txz";
	string roomDir = "/room";
	string zpoolName = "uninitialized-zpool-name";

	void downloadBase();
	void createRoomDir();
	string getRoomPathByName(const string& name);
	bool validateZfsPoolName(const string& name, string& errorMsg);
	string getZfsPoolName(const string& path);
};
