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
#include "setuidHelper.h"
#include "zfsPool.h"

Room::Room(const string& managerRoomDir, const string& name)
{
	roomDir = managerRoomDir;
	validateName(name);

	ownerUid = SetuidHelper::getActualUid();

	PasswdEntry pwent(ownerUid);
	ownerLogin = pwent.getLogin();

	roomDataDir = roomDir + "/" + ownerLogin + "/" + name;
	chrootDir = roomDataDir + "/root";
	useZfs = ZfsPool::detectZfs();
	if (useZfs) {
		string zpoolName = ZfsPool::getNameByPath(roomDir);
		//FIXME: these are two names for the same thing now..
		parentDataset = zpoolName + "/room/" + ownerLogin;
		roomDataset = zpoolName + "/room/" + ownerLogin;
	}
}

void Room::exec(std::vector<std::string> execVec)
{
	int jid = jail_getid(jailName.c_str());
	if (jid < 0) {
		log_debug("jail `%s' not running; will start it now", jailName.c_str());
		boot();
	}

	// Strange things happen if you enter the jail with uid != euid,
	// so lets do a sanity check. This should never happen.
	//if (getuid() != geteuid()) {
	//	throw std::runtime_error("uid mismatch");
	//}

	string x11_display = "DISPLAY=";
	string x11_xauthority = "XAUTHORITY=";
	string dbus_address = "DBUS_SESSION_BUS_ADDRESS=";
	string jail_username = "root";
	string env_username = "USER=root";
	if (roomOptions.isAllowX11Clients()) {
		if (getenv("DISPLAY")) x11_display += getenv("DISPLAY");
		if (getenv("XAUTHORITY")) x11_xauthority += getenv("XAUTHORITY");
		if (getenv("DBUS_SESSION_BUS_ADDRESS")) dbus_address += getenv("DBUS_SESSION_BUS_ADDRESS");
		jail_username = ownerLogin;
		env_username = "USER=" + ownerLogin;
	}

	std::vector<char*> argsVec = {
			(char*)"/usr/sbin/jexec",
			(char*) "-U",
			(char*) jail_username.c_str(),
			(char*) jailName.c_str(),
	};
	if (execVec.size() == 0) {
		argsVec.push_back((char*)"/bin/sh");
	} else {
		for (string& s : execVec) {
			char *tmp = strdup(s.c_str());
			argsVec.push_back(tmp);
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

	SetuidHelper::raisePrivileges();
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
	std::vector<std::string> argsVec;

	Room::exec(argsVec);
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

void Room::clone(const string& snapshot, const string& destRoom)
{
	log_debug("cloning room");

	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", {
			"clone",
			roomDataset + "/" + roomName + "@" + snapshot,
			parentDataset + "/" + destRoom
	});

	Room cloneRoom(roomDir, destRoom);
	SetuidHelper::lowerPrivileges();
	log_debug("clone complete");
}

void Room::create(const string& baseTarball)
{
	string cmd;

	SetuidHelper::raisePrivileges();

	log_debug("creating room");

	if (useZfs) {
		Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName});
		FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, (gid_t) ownerUid);
	} else {
		FileUtil::mkdir_idempotent(roomDataDir, 0700, ownerUid, (gid_t) ownerUid);
		FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, (gid_t) ownerUid);
	}

	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });

	PasswdEntry pwent(ownerUid);
	Shell::execute("/usr/sbin/pw", {
			"-R",  chrootDir,
			"user", "add",
			"-u", std::to_string(ownerUid),
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

	syncRoomOptions();

	if (useZfs) {
		Shell::execute("/sbin/zfs", {
				"snapshot",
				roomDataset + "/" + roomName + "@__initial"
		});
	}

	SetuidHelper::lowerPrivileges();

	log_debug("room %s created", roomName.c_str());
}

void Room::syncRoomOptions()
{
	roomOptions.writefile(roomDataDir + "/options.0");
}

void Room::boot() {
	int rv;

	roomOptions.readfile(roomDataDir + "/options.0");

	SetuidHelper::raisePrivileges();

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

	if (roomOptions.isShareTempDir()) {
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/tmp", chrootDir + "/tmp" });
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/var/tmp", chrootDir + "/var/tmp" });
	}

	if (roomOptions.isShareHomeDir()) {
		PasswdEntry pwent(ownerUid);
		Shell::execute("/sbin/mount", { "-t", "nullfs", pwent.getHome(), chrootDir + pwent.getHome() });
	}

	if (roomOptions.isUseLinuxAbi()) {
		// TODO: maybe load kernel modules? or maybe require that be done during system boot...
		//       requires:    kldload linux fdescfs linprocfs linsysfs tmpfs
		Shell::execute("/sbin/mount", { "-t", "linprocfs", "linprocfs", chrootDir + "/proc" });
		Shell::execute("/sbin/mount", { "-t", "linsysfs", "linsysfs", chrootDir + "/sys" });
	}
	SetuidHelper::lowerPrivileges();
}

void Room::destroy()
{
	string cmd;

	log_debug("destroying room at %s", chrootDir.c_str());

	SetuidHelper::raisePrivileges();

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

	SetuidHelper::lowerPrivileges();
	killAllProcesses();
	SetuidHelper::raisePrivileges();

	log_debug("unmounting /dev again, now that all processes are gone");
	if (unmount(string(chrootDir + "/dev").c_str(), MNT_FORCE) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
		}
	}

	if (true || roomOptions.isShareTempDir()) {
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

	if (roomOptions.isUseLinuxAbi()) {
		Shell::execute("/sbin/umount", { chrootDir + "/proc" });
		Shell::execute("/sbin/umount", { chrootDir + "/sys" });
	}

	if (roomOptions.isShareHomeDir()) {
		if (roomOptions.isShareHomeDir()) {
			PasswdEntry pwent(ownerUid);
			Shell::execute("/sbin/umount", { chrootDir + pwent.getHome() });
		}
	}

	if (jailExists()) {
		log_debug("removing jail");
		Shell::execute("/usr/sbin/jail", { "-r", jailName });
	} else {
		log_warning("jail(2) does not exist");
	}

	if (useZfs) {
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

	SetuidHelper::lowerPrivileges();

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

	SetuidHelper::raisePrivileges();

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
	SetuidHelper::lowerPrivileges();
}

void Room::exportArchive()
{
	const string snapName = "tmp_export";
	int result;

	if (!useZfs) {
		throw std::runtime_error("TODO -- not implemented yet");
	}

	string snapPath = roomDataset + "/" + roomName + "@" + snapName;

	Shell::execute("/sbin/zfs", { "snapshot", "-r", snapPath }, result);
	if (result != 0) {
		throw std::runtime_error("command failed: zfs snapshot");
	}

	Subprocess p;
	p.execute("/sbin/zfs", { "send", snapPath });
	result = p.waitForExit();
	if (result != 0) {
		sleep(2); // horrible way to avoid "snapshot is busy" error

		int ignored;
		Shell::execute("/sbin/zfs", { "destroy", snapPath }, ignored);
		std::cerr << "ZFS send failed with exit code " << result << "\n";
		throw std::runtime_error("command failed: zfs send");
	}

	sleep(2); // horrible way to avoid "snapshot is busy" error

	Shell::execute("/sbin/zfs", { "destroy", snapPath }, result);
	if (result != 0) {
		std::cerr << "Failed 2\n";
		throw std::runtime_error("command failed: zfs destroy");
	}
}

/*
void Room::setOsType(const string& osType)
{
	if (osType == "centos-6-x86") {
		installOpts.tarballUri = "http://download.openvz.org/template/precreated/centos-6-x86.tar.gz";
	} else if (osType == "freebsd-10-amd64") {
		installOpts.tarballUri = "http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/10.3-RELEASE/base.txz";
	} else {
		throw std::runtime_error("invalid value for OS type");
	}
}
*/
