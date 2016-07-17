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
#include "room.h"

FILE *logfile = NULL;
#include "logger.h"

class RoomManager {
public:
	void bootstrap();
	static bool isBootstrapComplete();
	void setup(uid_t uid) {
		if (!isBootstrapComplete()) {
			throw std::runtime_error("bootstrap is required");
		}
		ownerUid = uid;
		downloadBase();
		createRoomDir();
		zpoolName = getZfsPoolName("/room");
	}
	void createRoom(const string& name);
	void destroyRoom(const string& name);
	Room getRoomByName(const string& name);
	void listRooms();

private:
	uid_t ownerUid;
	string baseTarball = "/var/cache/room-base.txz";
	string baseUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/11.0-BETA1/base.txz";
	string roomDir = "/var/room";
	string zpoolName = "uninitialized-zpool-name";

	void downloadBase();
	void createRoomDir();
	string getRoomPathByName(const string& name);
	bool validateZfsPoolName(const string& name, string& errorMsg);
	string getZfsPoolName(const string& path);
};

bool RoomManager::isBootstrapComplete()
{
	return FileUtil::checkExists("/room");
}

void RoomManager::bootstrap()
{
	string zpool;
	for (;;) {
		cout << "Enter the ZFS pool that rooms will be stored in: ";
		cin >> zpool;
		string errorMsg;
		if (!validateZfsPoolName(zpool, errorMsg)) {
			cout << "Error: " << errorMsg << endl;
			continue;
		}
		if (Shell::executeWithStatus("zpool list " + zpool + " | grep -q " + zpool) != 0) {
			cout << "Error: no such ZFS pool" << endl;
			continue;
		}
		break;
	}

	Shell::execute("zfs create -o canmount=on -o mountpoint=/room " + zpool + "/room");
}

void RoomManager::createRoomDir() {
	FileUtil::mkdir_idempotent(roomDir, 0700, 0, 0);
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

string RoomManager::getZfsPoolName(const string& path)
{
	if (!FileUtil::checkExists(path)) {
		throw std::runtime_error("path does not exist: " + path);
	}

	string cmd = "df -h " + path + " | tail -1 | sed 's,/.*,,'";
	return Shell::popen_readline(cmd);
}

bool RoomManager::validateZfsPoolName(const string& name, string& errorMsg)
{
	string buf = name;
	std::locale loc("C");

	errorMsg = "";

	if (name.length() == 0) {
		errorMsg = "name cannot be empty";
		return false;
	}
	if (name.length() > 72) {
		errorMsg = "name is too long";
		return false;
	}
	for (std::string::iterator it = buf.begin(); it != buf.end(); ++it) {
		if (*it == '\0') {
			errorMsg = "NUL in name";
			return false;
		}
		if (std::isalnum(*it, loc) || strchr("-_", *it)) {
			// ok
		} else {
			errorMsg = "Illegal character in name: ";
			errorMsg.push_back(*it);
			return false;
		}
	}
	return true;
}

Room RoomManager::getRoomByName(const string& name)
{
	return Room(roomDir, name, ownerUid);
}

void RoomManager::createRoom(const string& name)
{
	log_debug("creating room");

	Room room(roomDir, name, ownerUid);
	room.create(baseTarball);
}

void RoomManager::destroyRoom(const string& name)
{
	string cmd;

	Room room(roomDir, name, ownerUid);
	room.destroy();
}

void RoomManager::listRooms()
{
	Shell::execute("ls -1 " + roomDir + "/" + std::to_string(ownerUid));
}

void usage() {
	std::cout <<
		"Usage:\n\n"
		"  room [create|destroy|enter] <name>\n"
		" -or-\n"
		"  room [bootstrap|list]\n"
		"\n"
		"  Miscellaneous options:\n\n"
		"    -h, --help         This screen\n"
	<< std::endl;
}

int
main(int argc, char *argv[])
{
	RoomManager mgr;
	char ch;
	static struct option longopts[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "version", no_argument, NULL, 'v' },
			{ NULL, 0, NULL, 0 }
	};

#if 0
	try {
		std::unique_ptr<libjob::jobdConfig> jc(new libjob::jobdConfig);
		jobd_config = jc.get();
	} catch (std::exception& e) {
		printf("ERROR: jobd_config: %s\n", e.what());
		exit(EXIT_FAILURE);
	} catch (...) {
		puts("ERROR: Unhandled exception when initializing jobd_config");
		exit(EXIT_FAILURE);
	}
#endif

	if (getenv("ROOM_DEBUG") == NULL) {
		logfile = fopen("/dev/null", "w");
	} else {
		logfile = stderr;
	}

	while ((ch = getopt_long(argc, argv, "hv", longopts, NULL)) != -1) {
		switch (ch) {
		case 'h':
			usage();
			return 0;
			break;

		case 'v':
			puts("FIXME: not implemented yet");
			return 0;
			break;

		default:
			usage();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	try {

		if (geteuid() != 0) {
			throw std::runtime_error("Insufficient permissions; must be run as root");
		}

		uid_t real_uid = getuid();
		if (getuid() == geteuid()) {
			const char* buf = getenv("SUDO_UID");
			if (buf) {
				real_uid = std::stoul(buf);
			} else {
				throw std::runtime_error("The root user is not allowed to create rooms. Use a normal user account instead");
			}
		}

		// Implicitly bootstrap OR (bootstrap explicitly AND exit)
		if (RoomManager::isBootstrapComplete()) {
			if (argc == 1 && string(argv[0]) == "bootstrap") {
				cout << "The bootstrap process is already complete. Nothing to do." << endl;
				exit(0);
			}
		} else {
			mgr.bootstrap();
			if (argc == 1 && string(argv[0]) == "bootstrap") {
				exit(0);
			}
		}

		mgr.setup(real_uid);
		log_debug("uid=%d euid=%d real_uid=%d", getuid(), geteuid(), real_uid);

		std::string command = std::string(argv[0]);
		if (argc == 1) {
			if (command == "list") {
				mgr.listRooms();
			} else if (command == "bootstrap") {
				throw std::logic_error("NOTREACHED");
			} else if (command == "--help" || command == "-h" || command == "help") {
				usage();
				exit(1);
			} else {
				throw std::runtime_error("Invalid command");
			}
		} else if (argc > 1) {
			std::string name = std::string(argv[1]);

			if (command == "create") {
				mgr.createRoom(name);
			} else if (command == "destroy") {
				mgr.destroyRoom(name);
			} else if (command == "enter") {
				mgr.getRoomByName(name).enter();
			} else {
				throw std::runtime_error("Invalid command");
			}
		}
	} catch(const std::system_error& e) {
		std::cout << "Caught system_error with code " << e.code()
	                  << " meaning " << e.what() << '\n';
		exit(1);
	} catch(const std::exception& e) {
		std::cout << "ERROR: Unhandled exception: " << e.what() << '\n';
		exit(1);
	} catch(...) {
		std::cout << "Unhandled exception\n";
		exit(1);
	}

	return EXIT_SUCCESS;
}
