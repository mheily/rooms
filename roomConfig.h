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

#include <unistd.h>

#include "namespaceImport.h"

class RoomConfig {
public:
	RoomConfig();

	const string& getParentDataset() const {
		return parentDataset;
	}

	void setParentDataset(const string& parentDataset) {
		this->parentDataset = parentDataset;
	}

	uid_t getOwnerUid() const {
		return ownerUid;
	}

	void setOwnerUid(uid_t ownerUid) {
		this->ownerUid = ownerUid;
	}

	const string& getRoomDir() const {
		return roomDir;
	}

	void setRoomDir(const string& roomDir = "/room") {
		this->roomDir = roomDir;
	}

	const string& getOwnerLogin() const {
		return ownerLogin;
	}

	void setOwnerLogin(const string& ownerLogin) {
		this->ownerLogin = ownerLogin;
	}

	bool useZfs() const {
		return b_useZfs;
	}

private:
	// the top-level directory for rooms
	string roomDir = "/room";
	string parentDataset;
	uid_t ownerUid;
	string ownerLogin;
	bool b_useZfs;
};
