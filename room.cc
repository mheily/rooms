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
#include "jail_getid.h"
#include "room.h"

Room::Room(const RoomConfig roomConfig, const string& managerRoomDir, const string& name)
{
	this->roomConfig = roomConfig;
	roomDir = managerRoomDir;
	validateName(name);
	chrootDir = roomDir + "/" + roomConfig.getOwnerLogin() + "/" + name;
	if (roomConfig.useZfs()) {
		roomDataset = roomConfig.getParentDataset();
	}
}

void Room::exec(int argc, char *argv[])
{
	int jid = jail_getid(jailName.c_str());
	if (jid < 0) {
		log_debug("jail `%s' not running; will start it now", jailName.c_str());
		boot();
	}

	// Strange things happen if you enter the jail with uid != euid,
	// so lets become root all the way
	if (setgid(0) < 0) {
		log_errno("setgid(2)");
		throw std::system_error(errno, std::system_category());
	}
	if (setuid(0) < 0) {
		log_errno("setuid(2)");
		throw std::system_error(errno, std::system_category());
	}

	string x11_display = "DISPLAY=";
	string x11_xauthority = "XAUTHORITY=";
	string dbus_address = "DBUS_SESSION_BUS_ADDRESS=";
	string jail_username = "root";
	string env_username = "USER=root";
	if (roomOptions->isAllowX11Clients()) {
		if (getenv("DISPLAY")) x11_display += getenv("DISPLAY");
		if (getenv("XAUTHORITY")) x11_xauthority += getenv("XAUTHORITY");
		if (getenv("DBUS_SESSION_BUS_ADDRESS")) dbus_address += getenv("DBUS_SESSION_BUS_ADDRESS");
		jail_username = roomConfig.getOwnerLogin();
		env_username = "USER=" + roomConfig.getOwnerLogin();
	}

	std::vector<char*> argsVec = {
			(char*)"/usr/sbin/jexec",
			(char*) "-U",
			(char*) jail_username.c_str(),
			(char*) jailName.c_str(),
	};
	if (argc == 0) {
		argsVec.push_back((char*)"/bin/sh");
	} else {
		char **p;

		for (p = argv + 2; *p != NULL; p++) {
			puts(*p);
			argsVec.push_back(const_cast<char*>(*p));
		}
	}
	argsVec.push_back(NULL);

	char* const envp[] = {
			(char*)"HOME=/",
			(char*)"SHELL=/bin/sh",
			(char*)"PATH=/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin",
			(char*)"TERM=xterm",
			(char*)"USER=root",
			(char*)x11_display.c_str(),
			(char*)x11_xauthority.c_str(),
			(char*)dbus_address.c_str(),
			NULL
	};

	if (execve("/usr/sbin/jexec", argsVec.data(), envp) < 0) {
		log_errno("execve(2)");
		throw std::system_error(errno, std::system_category());
	}
	//NOTREACHED
	log_errno("execve(2)");
	throw std::runtime_error("exec failed");

//DEADWOOD: easier to use jexec here, but this works for systems w/o jails
#if 0
	gid_t guestGid = (gid_t) guestUid;


	if (chdir(chrootDir.c_str()) < 0) {
		log_errno("chdir(2) to `%s'", chrootDir.c_str());
		throw std::system_error(errno, std::system_category());
	}
	if (chroot(chrootDir.c_str()) < 0) {
		log_errno("chroot(2) to `%s'", chrootDir.c_str());
		throw std::system_error(errno, std::system_category());
	}
	if (setgroups(1, &guestGid) < 0) {
		log_errno("setgroups(2)");
		throw std::system_error(errno, std::system_category());
	}
	if (setgid(guestGid) < 0) {
		log_errno("setgid(2)");
		throw std::system_error(errno, std::system_category());
	}
	if (setuid(guestUid) < 0) {
		log_errno("setuid(2)");
		throw std::system_error(errno, std::system_category());
	}
	char *const args[] = { (char*)"/bin/sh", NULL };
	if (execv("/bin/sh", args) < 0) {
		log_errno("execv(2)");
		throw std::system_error(errno, std::system_category());
	}
	abort();
#endif
}

void Room::enter() {
	int argc = 0;
	char *argv[] = { NULL };

	Room::exec(argc, argv);
}

bool Room::jailExists()
{
	int jid = jail_getid(jailName.c_str());
	log_debug("jail `%s' has jid %d", jailName.c_str(), jid);
	return (jid >= 0);
}

void Room::validateName(const string& name)
{
	string buf = name;
	std::locale loc("C");

	//FIXME: this function does too much.. should have a separate function
	// for generating roomName and jailName
	// Having all this here creates subtle ordering bugs.
	roomName = name;
	jailName = "room_" + roomConfig.getOwnerLogin() + "_";

	if (name.length() == 0) {
		throw std::runtime_error("name cannot be empty");
	}
	if (name.length() > 72) {
		throw std::runtime_error("name is too long");
	}
	if (name.at(0) == '.' || name.at(0) == '_') {
		throw std::runtime_error("names may not start with a dot or underscore");
	}
	for (std::string::iterator it = buf.begin(); it != buf.end(); ++it) {
		if (*it == '\0') {
			throw std::runtime_error("NUL in name");
		}
		if (std::isalnum(*it, loc) || strchr("-_.", *it)) {
			if (*it == '.') {
				jailName.push_back('^');
			} else {
				jailName.push_back(*it);
			}
		} else {
			cout << "Illegal character in name: " << *it << endl;
			throw std::runtime_error("invalid character in name");
		}
	}
}

void Room::clone(const string& snapshot, const string& destRoom)
{
	log_debug("cloning room");

	Shell::execute("/sbin/zfs", {
			"clone",
			roomDataset + "/" + roomName + "@" + snapshot,
			roomConfig.getParentDataset() + "/" + destRoom
	});

	Room cloneRoom(roomConfig, roomDir, destRoom);
}

void Room::create(const string& baseTarball)
{
	string cmd;

	log_debug("creating room");

	if (roomConfig.useZfs()) {
		Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName});
	} else {
		FileUtil::mkdir_idempotent(chrootDir, 0700, roomConfig.getOwnerUid(), (gid_t) roomConfig.getOwnerUid());
	}

	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });

	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });
	PasswdEntry pwent(roomConfig.getOwnerUid());
	Shell::execute("/usr/sbin/pw", {
			"-R",  chrootDir,
			"user", "add",
			"-u", std::to_string(roomConfig.getOwnerUid()),
			"-n", pwent.getLogin(),
			"-c", pwent.getGecos(),
			"-s", pwent.getShell(),
			"-G", "wheel",
			"-m"
	});

	// KLUDGE: copy /etc/resolv.conf
	Shell::execute("/bin/cp", {
			"/etc/resolv.conf",
			chrootDir + "/etc/resolv.conf"
	});

	// KLUDGE: install pkg(8)
	Shell::execute("/usr/sbin/chroot", {
			"-u", "root",
			chrootDir,
			"env", "ASSUME_ALWAYS_YES=YES", "pkg", "bootstrap"
	});

	if (roomConfig.useZfs()) {
		Shell::execute("/sbin/zfs", {
				"snapshot",
				roomDataset + "/" + roomName + "@__initial"
		});
	}

	log_debug("room %s created", roomName.c_str());
}

void Room::boot() {
	int rv;

	Shell::execute("/usr/sbin/jail", {
			"-i",
			"-c", "name=" + jailName,
			"host.hostname=" + roomName + ".room",
			"path=" + chrootDir,
			"ip4=inherit",
			"mount.devfs",
#if __FreeBSD__ >= 11
			"sysvmsg=new",
			"sysvsem=new",
			"sysvshm=new",
#endif
			"persist",
	}, rv);
	if (rv != 0) {
		log_error("jail(1) failed; rv=%d", rv);
		throw std::runtime_error("jail(1) failed");
	}

	if (roomOptions->isShareTempDir()) {
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/tmp", chrootDir + "/tmp" });
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/var/tmp", chrootDir + "/var/tmp" });
	}
}

void Room::destroy()
{
	string cmd;

	log_debug("destroying room at %s", chrootDir.c_str());
	if (chrootDir == "" || chrootDir == "/") {
		throw std::runtime_error("bad chrootDir");
	}

	if (!FileUtil::checkExists(chrootDir)) {
		throw std::runtime_error("room does not exist");
	}

	log_debug("unmounting /dev");
	if (unmount(string(chrootDir + "/dev").c_str(), MNT_FORCE) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
		}
	}

	killAllProcesses();

	log_debug("unmounting /dev again, now that all processes are gone");
	if (unmount(string(chrootDir + "/dev").c_str(), MNT_FORCE) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
		}
	}

	if (roomOptions->isShareTempDir()) {
		log_debug("unmounting /tmp");
		if (unmount(string(chrootDir + "/tmp").c_str(), MNT_FORCE) < 0) {
			if (errno != EINVAL) {
				log_errno("unmount(2)");
				throw std::system_error(errno, std::system_category());
			}
		}
		log_debug("unmounting /var/tmp");
		if (unmount(string(chrootDir + "/var/tmp").c_str(), MNT_FORCE) < 0) {
			if (errno != EINVAL) {
				log_errno("unmount(2)");
				throw std::system_error(errno, std::system_category());
			}
		}
	}

	if (jailExists()) {
		log_debug("removing jail");
		Shell::execute("/usr/sbin/jail", { "-r", jailName });
	} else {
		log_warning("jail(2) does not exist");
	}

	if (roomConfig.useZfs()) {
		// this races with "jail -r"
		bool success = false;
		for (int i = 0; i < 5; i++) {
			sleep(1);
			int result;
			Shell::execute("/sbin/zfs", { "destroy", roomDataset + "/" + roomName}, result);
			if (result == 0) {
				success = true;
				break;
			}
		}
		if (!success) {
			log_error("unable to destroy the ZFS dataset");
			throw std::runtime_error("unable to destroy the ZFS dataset");
		}

	} else {
		// remove the immutable flag
		Shell::execute("/bin/chflags", {"-R", "noschg", chrootDir});

		// remove all files (this makes me nervous)
		Shell::execute("/bin/rm", {"-rf", chrootDir});
	}

	log_notice("room deleted");
}

void Room::killAllProcesses()
{
	log_debug("killing all processes for room %s", roomName.c_str());

	// TODO: have a "nice" option that uses SIGTERM as well

	int jid = jail_getid(jailName.c_str());

	if (jid < 0) {
		log_debug("jail does not exist; skipping");
		return;
	}

	// Kill all programs still using the filesystem
	int status;
	Shell::execute("/bin/pkill", { "-9", "-j", std::to_string(jid)}, status);
	if (status > 1) {
		log_error("pkill failed with status %d", status);
		throw std::runtime_error("pkill failed");
	}

	bool timeout = true;
	for (int i = 0 ; i < 60; i++) {
		int status;
		Shell::execute("/bin/pgrep", { "-q", "-j", std::to_string(jid) }, status);
		if (status > 1) {
			log_error("pkill failed");
			throw std::runtime_error("pkill failed");
		}
		if (status == 0) {
			log_debug("waiting for processes to die");
			sleep(1);
		}
		if (status == 1) {
			timeout = false;
			break;
		}
	}
	if (timeout) {
		log_error("pkill failed; timeout reached");
		throw std::runtime_error("processes will not die");
	}
}
