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
#include "passwdEntry.h"
#include "roomConfig.h"
#include "roomOptions.h"

extern FILE *logfile;
#include "logger.h"

class Room {
public:
	Room(const RoomConfig roomConfig, const string& managerRoomDir, const string& name);

	void create(const string& baseTarball);
	void clone(const string& snapshot, const string& destRoom);
	void killAllProcesses();
	void boot();
	void destroy();
	void enter();
	void exec(std::vector<std::string> execVec);

	void setRoomOptions(RoomOptions& roomOptions) {
		this->roomOptions = &roomOptions;
	}

	RoomOptions* getRoomOptions() const {
		return roomOptions;
	}

private:
	RoomConfig roomConfig;
	RoomOptions* roomOptions;
	string roomDir;   // copy of RoomManager::roomDir
	string roomDataDir; // directory where this rooms data is stored
	string chrootDir; // path to the root of the chroot environment
	string roomName;  // name of this room
	string jailName;  // name of the jail

	string roomDataset; // the name of the ZFS dataset for the room

	bool jailExists();
	void customizeWithoutRoot();
	void validateName(const string& name);
};
