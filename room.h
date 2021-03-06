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

#include <memory>

#include "namespaceImport.h"
#include "Container.hpp"
#include "roomOptions.h"

extern FILE *logfile;
#include "logger.h"

struct RoomInstallParams {
	string name;
	string roomDir;
	string installRoot;
	string baseArchiveUri;
	RoomOptions options;
};

enum e_RoomState {
	ROOM_STATE_UNKNOWN,
	ROOM_STATE_DEFINED,
	ROOM_STATE_MOUNTED,
	ROOM_STATE_RUNNING,
};

class Room {
public:
	Room(const string& managerRoomDir, const string& name);
	static void install(const struct RoomInstallParams& rip);
	void createEmpty();
	void editConfiguration();
	void extractTarball(const string& baseTarball);
	int forkAndExec(std::vector<std::string> execVec, const string& runAsUser);
	void clone(const string& snapshot, const string& destRoom, const RoomOptions& roomOpt);
	void killAllProcesses();
	void snapshotCreate(const string& name);
	void snapshotDestroy(const string& name);
	void snapshotReceive(const string& name);
	void start();
	void stop();
	void destroy();
	static void destroy(const string& name);
	void enter();
	void send();
	void mount();
	void unmount();
	void exec(std::vector<std::string> execVec, const string& runAsUser);
	void exportArchive();
	void printSnapshotList();
	void transitionState(enum e_RoomState targetState);

	// Remote push/pull functions
	//void cloneFromOrigin(const string& uri);
	//void pushToOrigin();
	void setOriginUri(const string& uri);

	// Cloned datasets need to use "zfs promote" in conjunction with "zfs receive"
	void promoteClone(const string& cloneDataset);

	static bool isValidName(const string& name)
	{
		try {
			Room::validateName(name);
		} catch (...) {
			return false;
		}
		return true;
	}

	// The following methods can be used after create() but before install()
	void setOsType(const string& osType);

	RoomOptions& getRoomOptions() {
		if (! areRoomOptionsLoaded ) {
			throw std::logic_error("options not loaded");
		}
		return roomOptions;
	}

	void loadRoomOptions() {
		roomOptions.load(roomOptionsPath);
		areRoomOptionsLoaded = true;
	}

	void syncRoomOptions();

	string getLatestSnapshot();

private:
	//std::unique_ptr<Container> container = std::make_unique<Container>(Container::create());
	Container* container;
	bool areRoomOptionsLoaded = false;
	RoomOptions roomOptions;
	string roomDir;   // copy of RoomManager::roomDir
	string roomDataDir; // directory where this rooms data is stored
	string chrootDir; // path to the root of the chroot environment
	string roomName;  // name of this room
	string jailName;  // name of the jail
	uid_t ownerUid; // the uid of the jail owner
	gid_t ownerGid; // the gid of the jail owner
	string ownerLogin; // the login name from passwd(5) for the jail owner
	string parentDataset; // the parent of roomDataset
	string roomDataset; // the name of the ZFS dataset for the room
	string zpoolName; // the ZFS pool the room lives in
	string roomOptionsPath; // The path to the room options.json file
	bool useZfs; // if true, create ZFS rooms
	enum e_RoomState state = ROOM_STATE_UNKNOWN;

/*	struct {
		string tarballUri; // URI to the tarball for the root fs
	} installOpts;*/

	bool isClone() const {
		return (roomOptions.templateSnapshot != "");
	}

	void determineInitialState();
	void enterJail(const string& runAsUser);
	bool jailExists();
	void customizeWithoutRoot();
	static void validateName(const string& name);
	void pushResolvConf();
	void getJailName();
	static void parseRemoteUri(const string& uri, string& scheme, string& host, string& path);
	string generateSnapshotName();
};
