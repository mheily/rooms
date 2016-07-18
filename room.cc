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

Room::Room(const string& managerRoomDir, const string& name, uid_t uid, const string& dataset)
{
	PasswdEntry pwent(uid);

	roomDir = managerRoomDir;
	ownerUid = uid;
	ownerLogin = pwent.getLogin();
	validateName(name);
	chrootDir = roomDir + "/" + ownerLogin + "/" + name;
	if (dataset == "") {
		useZFS = false;
	} else {
		useZFS = true;
		roomDataset = dataset;
	}

}

void Room::enter()
{
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
	if (allowX11Clients) {
		x11_display += getenv("DISPLAY");
		x11_xauthority += getenv("XAUTHORITY");
		dbus_address += getenv("DBUS_SESSION_BUS_ADDRESS");
		jail_username = ownerLogin;
		env_username = "USER=" + ownerLogin;
		enableX11Clients();
	}

	char* const args[] = {
			(char*)"/usr/sbin/jexec",
			(char*) "-U",
			(char*) jail_username.c_str(),
			(char*) jailName.c_str(),
			(char*)"/bin/sh",
			NULL
	};

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

	if (execve("/usr/sbin/jexec", args, envp) < 0) {
		log_errno("execve(2)");
		throw std::system_error(errno, std::system_category());
	}
	abort();

//DEADWOOD: easier to use jexec here
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

bool Room::jailExists()
{
	if (Shell::executeWithStatus("jls -j " + jailName + " >/dev/null") == 0) {
		return true;
	} else {
		return false;
	}
}

void Room::validateName(const string& name)
{
	string buf = name;
	std::locale loc("C");

	//FIXME: this function does too much.. should have a separate function
	// for generating roomName and jailName
	// Having all this here creates subtle ordering bugs.
	roomName = name;
	jailName = "room_" + ownerLogin + "_";

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

void Room::enableX11Clients() {
	Shell::execute("cp /var/tmp/.PCDMAuth-* " + chrootDir + "/var/tmp");

	// FIXME: SECURITY
	// Despite having the cookie, clients fail with 'No protocol specified'
	// This workaround is bad for security in a multi-user system, but gets things going for a desktop.
	Shell::execute("xhost +local >/dev/null");
}

void Room::clone(const string& srcRoom, const string& destRoom)
{
	log_debug("cloning room");
	abort();
}

void Room::create(const string& baseTarball)
{
	string cmd;

	log_debug("creating room");

	if (useZFS) {
		Shell::execute("zfs create " + roomDataset + "/" + roomName);
	} else {
		FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, (gid_t) ownerUid);
	}

	Shell::execute("tar -C " + chrootDir + " -xf " + baseTarball);

	PasswdEntry pwent(ownerUid);
	Shell::execute(string("pw -R " + chrootDir + " user add -u " + std::to_string(ownerUid) +
			" -n " + pwent.getLogin() +
			" -c " + pwent.getGecos() +
			" -s " + pwent.getShell() +
			" -G wheel" +
			" -m")
			);

	// KLUDGE: copy /etc/resolv.conf
	Shell::execute("cp /etc/resolv.conf " + chrootDir + "/etc/resolv.conf");

	// KLUDGE: install pkg(8)
	Shell::execute("chroot -u root " + chrootDir + " env ASSUME_ALWAYS_YES=YES pkg bootstrap > /dev/null");

	Shell::execute(
			"jail -i -c name=" + jailName +
			" host.hostname=" + roomName + ".room" +
			" path=" + chrootDir +
			" ip4=inherit" +
			" mount.devfs" +
			" sysvmsg=new sysvsem=new sysvshm=new" +
			" persist"
			" >/dev/null"
	);

	if (shareTempDir) {
		Shell::execute("mount -t nullfs /tmp " + chrootDir + "/tmp");
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

	if (shareTempDir) {
		log_debug("unmounting /tmp");
		if (unmount(string(chrootDir + "/tmp").c_str(), MNT_FORCE) < 0) {
			if (errno != EINVAL) {
				log_errno("unmount(2)");
				throw std::system_error(errno, std::system_category());
			}
		}
	}

	if (jailExists()) {
		log_debug("removing jail");
		Shell::execute("jail -r " + jailName);
	} else {
		log_warning("jail(2) does not exist");
	}

	// remove the immutable flag
	Shell::execute("chflags -R noschg " + chrootDir);

	if (useZFS) {
		// this races with "jail -r"
		bool success = false;
		for (int i = 0; i < 5; i++) {
			sleep(1);
			int result = Shell::executeWithStatus("zfs destroy " + roomDataset + "/" + roomName);
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
		// remove all files (this makes me nervous)
		Shell::execute("rm -rf " + chrootDir);
	}

	log_notice("room deleted");
}

void Room::killAllProcesses()
{
	log_debug("killing all processes for room %s", roomName.c_str());

	// TODO: have a "nice" option that uses SIGTERM as well

	if (!jailExists()) {
		log_debug("jail does not exist; skipping");
		return;
	}

	// Kill all programs still using the filesystem
	int status = Shell::executeWithStatus("pkill -9 -j " + jailName);
	if (status > 1) {
		log_error("pkill failed");
		throw std::runtime_error("pkill failed");
	}

	bool timeout = true;
	for (int i = 0 ; i < 60; i++) {
		int status = Shell::executeWithStatus("pgrep -q -j " + jailName);
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
