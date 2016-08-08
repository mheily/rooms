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
#include <dirent.h>
#include <getopt.h>
#include <jail.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <unistd.h>
}

#include "logger.h"
#include "namespaceImport.h"
#include "shell.h"
#include "fileUtil.h"
#include "room.h"
#include "roomConfig.h"
#include "roomManager.h"
#include "zfsPool.h"

string RoomManager::getUserRoomDir() {
	PasswdEntry pwent(roomConfig.getOwnerUid());
	string login = pwent.getLogin();

	return string(roomDir + "/" + pwent.getLogin());
}

bool RoomManager::isBootstrapComplete() {
	return FileUtil::checkExists(getUserRoomDir());
}

//DEADWOOD
void RoomManager::updateRoomConfig() {
}

void RoomManager::bootstrap() {
	string zpool;

	if (!FileUtil::checkExists(roomDir)) {

//		for (;;) {
//			cout << "Enter the ZFS pool that rooms will be stored in: ";
//			cin >> zpool;
//			string errorMsg;
//			if (!validateZfsPoolName(zpool, errorMsg)) {
//				cout << "Error: " << errorMsg << endl;
//				continue;
//			}
//			int rv = Shell::execute("/sbin/zfs", {	"list", zpool }, rv);
//			if (rv != 0) {
//				cout << "Error: no such ZFS pool" << endl;
//				continue;
//			}
//			break;
//		}

		if (roomConfig.useZfs()) {
			zpool = ZfsPool::getNameByPath("/");
			Shell::execute("/sbin/zfs", {
					"create",
					"-o", "canmount=on",
					"-o", "mountpoint=/room",
					zpool + "/room"});
		} else {
			FileUtil::mkdir_idempotent(roomDir, 0700, 0, 0);
		}
	} else {
		zpool = ZfsPool::getNameByPath(roomDir);
	}

	if (!FileUtil::checkExists(getUserRoomDir())) {
		if (roomConfig.useZfs()) {
		Shell::execute("/sbin/zfs",
				{ "create", zpool + "/room/" + roomConfig.getOwnerLogin() });
		} else {
			FileUtil::mkdir_idempotent(roomDir + "/" + roomConfig.getOwnerLogin(), 0700, 0, 0);
		}
	}

	updateRoomConfig();
	createBaseTemplate();
}

void RoomManager::setup() {
	if (!isBootstrapComplete()) {
		throw std::runtime_error("bootstrap is required");
	}

	updateRoomConfig();
}

void RoomManager::downloadBase() {
	if (!FileUtil::checkExists(baseTarball)) {
		cout << "Downloading base.txz..\n";
		string cmd = "fetch -o " + baseTarball + " " + baseUri;
		if (system(cmd.c_str()) != 0) {
			throw std::runtime_error("Download failed");
		}
	}
}

Room RoomManager::getRoomByName(const string& name) {
	Room room(roomConfig, roomDir, name);
	room.setRoomOptions(roomOptions);
	return room;
}

void RoomManager::createRoom(const string& name) {
	log_debug("creating room");

	Room room(roomConfig, roomDir, name);
	room.setRoomOptions(roomOptions);
	createBaseTemplate();
	room.create(baseTarball);
}

void RoomManager::cloneRoom(const string& src, const string& dest) {
	if (roomConfig.useZfs()) {
		Room srcRoom(roomConfig, roomDir, src);
		srcRoom.setRoomOptions(roomOptions);
		srcRoom.clone("__initial", dest);
	} else {
		// XXX-FIXME assumes we are cloning a template
		createRoom(dest);
	}
}

void RoomManager::cloneRoom(const string& dest) {
	cloneRoom(getBaseTemplateName(), dest);
}

void RoomManager::destroyRoom(const string& name) {
	string cmd;

	Room room(roomConfig, roomDir, name);
	room.setRoomOptions(roomOptions);
	room.destroy();
}

void RoomManager::listRooms() {
	if (!FileUtil::checkExists(getUserRoomDir())) {
		// FIXME: will never happen b/c bootstrap ensures the directory exists
		std::clog << "No rooms exist. Run 'room create' to create a room."
				<< endl;
	} else {
		DIR* dir;
		struct dirent* dp;

		dir = opendir(getUserRoomDir().c_str());
		if (dir == NULL) {
			log_errno("opendir(3)");
			throw std::system_error(errno, std::system_category());
		}

		string baseTemplateName = getBaseTemplateName();
		std::vector<string> roomVec;
		while ((dp = readdir(dir)) != NULL) {
			if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
				continue;
			}
			if (baseTemplateName == dp->d_name) {
				continue;
			}
			roomVec.push_back(string(dp->d_name));
		}
		closedir(dir);

		std::sort(roomVec.begin(), roomVec.end());

		for (string& s : roomVec) {
			cout << s << endl;
		}
	}
}

string RoomManager::getUserRoomDataset() {
	if (roomConfig.useZfs()) {
		return string(roomConfig.getZpoolName() + "/" + ownerLogin);
	} else {
		throw std::logic_error("ZFS not enabled");
	}
}

bool RoomManager::checkRoomExists(const string& name) {
	return FileUtil::checkExists(getUserRoomDir() + "/" + name);
}

string RoomManager::getBaseTemplateName() {
	struct utsname uts;

	if (uname(&uts) < 0) {
		log_errno("uname(3)");
		throw std::system_error(errno, std::system_category());
	}

	return string(uts.sysname) + "-" + string(uts.release);
}

void RoomManager::createBaseTemplate() {
	string base_template = getBaseTemplateName();

	if (checkRoomExists(base_template)) {
		return;
	}

	downloadBase();

	log_debug("creating base template: %s", base_template.c_str());

	Room room(roomConfig, roomDir, base_template);
	room.create(baseTarball);
}

static void printUsage() {
	std::cout <<
		"Usage:\n\n"
		"  room <name> [create|destroy|enter]\n"
		" -or-\n"
		"  room <name> exec <arg0..argN>\n"
		" -or-\n"
		"  room [bootstrap|list]\n"
		"\n"
		"  Miscellaneous options:\n\n"
		"    -h, --help         This screen\n"
		"    -v, --verbose      Increase verbosity level\n"
	<< std::endl;
}

void RoomManager::getOptions(int argc, char *argv[]) {
	string room_name = "";

	// Find the first positional argument
	int i;
	for (i = 1; i < argc; i++) {
		string context = argv[i];
		if (context == "bootstrap") { }
		else if (context == "list") {
			listRooms();
			exit(0);
		} else if (context == "help" || context == "--help" || context == "-h") {
			printUsage();
			exit(0);
		} else if (context == "--verbose" || context == "-v") {
			fclose(logfile);
			logfile = stderr;
			continue;
//		} else if (context.at(0) == '-') {
//			continue;
		} else {
			room_name = context;
			break;
		}
	}

	if (i == argc) {
		cout << "ERROR: must specify an action\n";
		printUsage();
		exit(1);
	}
	string command = argv[++i];

	if (command == "create") {
		getRoomOptions(argc, argv);
		cloneRoom(room_name);
	} else if (command == "destroy") {
		destroyRoom(room_name);
	} else if (command == "enter") {
		getRoomOptions(argc, argv); // FIXME: read the configuration file instead
		getRoomByName(room_name).enter();
	} else if (command == "exec") {
		getRoomOptions(argc, argv); // FIXME: read the configuration file instead

		std::vector<std::string> execVec;
		for (i++; i < argc; i++) {
			execVec.push_back(argv[i]);
		}
		if (execVec.size() == 0) {
			cout << "ERROR: must specify a command to execute\n";
			exit(1);
		}
		getRoomByName(room_name).exec(execVec);
	} else {
		throw std::runtime_error("Invalid command");
	}
}
