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

#include "namespaceImport.h"
#include "roomConfig.h"

extern FILE *logfile;
#include "logger.h"

class RoomManager {
public:
	RoomManager(RoomConfig roomConfig) {
		this->roomConfig = roomConfig;
	}
	void bootstrap();
	bool isBootstrapComplete();
	void setup();
	void createRoom(const string& name);
	void cloneRoom(const string& src, const string& dest);
	void cloneRoom(const string& dest);
	void destroyRoom(const string& name);
	Room getRoomByName(const string& name);
	void listRooms();

private:
	RoomConfig roomConfig;
	string ownerLogin;
	string baseTarball = "/var/cache/room-base.txz";
	string baseUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/11.0-BETA1/base.txz";
	string roomDir = "/room";
	string zpoolName;// = "uninitialized-zpool-name";

	void downloadBase();
	void createRoomDir();
	bool checkRoomExists(const string&);
	string getUserRoomDir();
	string getUserRoomDataset();
	string getRoomPathByName(const string& name);
	void updateRoomConfig();
	bool validateZfsPoolName(const string& name, string& errorMsg);
	string getZfsPoolName(const string& path);

	// templates
	string getBaseTemplateName();
	void createBaseTemplate();
};
