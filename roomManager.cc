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
#include "roomManager.h"
#include "zfsPool.h"

string RoomManager::getUserRoomDir() {
	PasswdEntry pwent(ownerUid);
	string login = pwent.getLogin();

	return string(roomDir + "/" + pwent.getLogin());
}

bool RoomManager::isBootstrapComplete() {
	string dir = getUserRoomDir();
	log_debug("checking if %s exists", dir.c_str());
	return FileUtil::checkExists(dir);
}

bool RoomManager::doesBaseTemplateExist() {
	return checkRoomExists(getBaseTemplateName());
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

		if (useZfs) {
			zpool = ZfsPool::getNameByPath("/");
			SetuidHelper::raisePrivileges();
			Shell::execute("/sbin/zfs", {
					"create",
					"-o", "canmount=on",
					"-o", "mountpoint=/room",
					zpool + "/room"});
			SetuidHelper::lowerPrivileges();
		} else {
			FileUtil::mkdir_idempotent(roomDir, 0700, 0, 0);
		}
	} else {
		zpool = ZfsPool::getNameByPath(roomDir);
	}

	if (!FileUtil::checkExists(getUserRoomDir())) {
		if (useZfs) {
			SetuidHelper::raisePrivileges();
		Shell::execute("/sbin/zfs",
				{ "create", zpool + "/room/" + ownerLogin });
			SetuidHelper::lowerPrivileges();
		} else {
			FileUtil::mkdir_idempotent(roomDir + "/" + ownerLogin, 0700, 0, 0);
		}
	}

	createBaseTemplate();
}

void RoomManager::downloadBase() {
	if (!FileUtil::checkExists(baseTarball)) {
		cout << "Downloading base.txz..\n";
		SetuidHelper::raisePrivileges();
		Shell::execute("/usr/bin/fetch",
				{ "-o", baseTarball, baseUri });
		SetuidHelper::lowerPrivileges();
                //FIXME: error checking for the above command
	}
}

Room& RoomManager::getRoomByName(const string& name) {
	enumerateRooms();
	auto it = rooms.find(name);
	if (it != rooms.end()) {
	    Room* r = rooms[name];
	    r->loadRoomOptions();
	    return *r;
	} else {
		throw std::runtime_error("Room " + name + " does not exist");
	}
}

void RoomManager::createRoom(const string& name) {
	log_debug("creating room");

	Room room(roomDir, name);
	createBaseTemplate();
	room.create(baseTarball);
}

void RoomManager::cloneRoom(const string& src, const string& dest) {
	if (useZfs) {
		Room* srcRoom = new Room(roomDir, src);
		srcRoom->clone("__initial", dest);
		delete srcRoom;
		enumerateRooms();
	} else {
		// XXX-FIXME assumes we are cloning a template
		createRoom(dest);

		// FIXME: below is untested
		Room* r = new Room(roomDir, src);
		rooms.insert(std::make_pair(dest, r));
		abort(); // fix the above code later
	}
}

void RoomManager::importRoom(const string& roomName) {
	const string snapName = "tmp_export";
	int result;

	if (!useZfs) {
		throw std::runtime_error("TODO -- not implemented yet");
	}

	string snapPath = getUserRoomDataset() + "/" + roomName;

	Subprocess p;
	p.execute("/sbin/zfs", { "receive", snapPath });
	if (p.waitForExit() != 0) {
		std::cerr << "command failed: zfs receive" << endl;
		exit(1);
	}

	Shell::execute("/sbin/zfs", { "destroy", snapPath + "@" + snapName }, result);
	if (result != 0) {
		std::cerr << "failed to destroy snapshot: " << snapName << endl;
		exit(1);
	}
}

void RoomManager::cloneRoom(const string& dest) {
	cloneRoom(getBaseTemplateName(), dest);
}

void RoomManager::destroyRoom(const string& name) {
	string cmd;

	Room room(roomDir, name);
	room.destroy();
}

void RoomManager::enumerateRooms() {
	DIR* dir;
	struct dirent* dp;

	if (!isBootstrapComplete()) {
		throw std::logic_error("tried to list rooms before bootstrapping");
	}

	log_debug("scanning rooms in %s", getUserRoomDir().c_str());
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
		string roomName = string(dp->d_name);

		if (baseTemplateName == roomName) {
			continue;
		}

		auto it = rooms.find(roomName);
		if (it != rooms.end()) {
			continue;
		}

		Room* r = new Room(roomDir, roomName);
		rooms.insert(std::make_pair(roomName, r));
	}
	closedir(dir);
}

// FIXME: make this use this->rooms instead
void RoomManager::listRooms() {
	enumerateRooms();
	if (rooms.empty()) {
		// FIXME: will never happen b/c bootstrap ensures the directory exists
		std::cerr << "No rooms exist. Run 'room create' to create a room."
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
	if (useZfs) {
		return string(ZfsPool::getNameByPath(roomDir) + "/room/" + ownerLogin);
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

	Room room(roomDir, base_template);
	RoomOptions roomOpt = room.getRoomOptions();
	roomOpt.allowX11Clients = true;
	roomOpt.shareTempDir = true;
	room.create(baseTarball);
}
