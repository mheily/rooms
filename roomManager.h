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

#include <map>

#include "namespaceImport.h"
#include "passwdEntry.h"
#include "roomOptions.h"
#include "setuidHelper.h"
#include "zfsPool.h"

extern FILE *logfile;
#include "logger.h"

#include "roomManagerUserOptions.h"

class RoomManager {
public:
	RoomManager() {
		useZfs = ZfsPool::detectZfs();
		ownerUid = SetuidHelper::getActualUid();
		ownerLogin = PasswdEntry(ownerUid).getLogin();
		userOptionsPath = string(PasswdEntry(ownerUid).getHome()) + "/.room/config.json";
	}
	void bootstrap();
	bool isBootstrapComplete();
	void initUserRoomSpace();
	bool doesBaseTemplateExist();
	void createBaseTemplate();
	void installRoom(const string& name, const string& archive, const RoomOptions& options);
	void importRoom(const string& name);
	void createRoom(const string& name);
	void cloneRoom(const string& src, const string& dest);
	void cloneRoom(const string& dest);
	void destroyRoom(const string& name);
	Room& getRoomByName(const string& name);
	bool checkRoomExists(const string&);
	bool checkTemplateExists(const string& name);
	void listRooms();

	void parseConfig();

	bool isVerbose() const {
		return verbose;
	}

	void setVerbose(bool verbose = false) {
		this->verbose = verbose;
		fclose(logfile);
		if (verbose) {
			logfile = stderr;
		} else {
			logfile = fopen("/dev/null", "w");
		}
	}

private:
	std::map<std::string, Room*> rooms;
	bool verbose = false;
	bool useZfs;
	uid_t ownerUid;
	RoomOptions roomOptions;
	RoomManagerUserOptions userOptions;
	string userOptionsPath;
	string ownerLogin;
	string baseTarball = "/var/cache/room-base.txz";
	//string baseUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/11.0-BETA1/base.txz";
	string baseUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/10.2-RELEASE/base.txz";
	string roomDir = "/room";

	void enumerateRooms();
	void downloadBase();
	void createRoomDir();
	string getUserRoomDir();
	string getUserRoomDataset();
	string getRoomPathByName(const string& name);

	// templates
	string getBaseTemplateName();

	int config_home_fd = -1; // descriptor opened to $HOME/.room
	void openConfigHome();
	void generateUserConfig();

};
