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
	ownerUid = SetuidHelper::getActualUid();
	ownerGid = (gid_t) ownerUid; // FIXME: bad assumption

	PasswdEntry pwent(ownerUid);
	ownerLogin = pwent.getLogin();

	validateName(name);
	getJailName();

	roomDataDir = roomDir + "/" + ownerLogin + "/" + name;
	chrootDir = roomDataDir + "/share/root";
	useZfs = ZfsPool::detectZfs();
	if (useZfs) {
		zpoolName = ZfsPool::getNameByPath(roomDir);
		//FIXME: these are two names for the same thing now..
		parentDataset = zpoolName + "/room/" + ownerLogin;
		roomDataset = zpoolName + "/room/" + ownerLogin;
	}
	roomOptionsPath = roomDataDir + "/etc/options.json";
	if (FileUtil::checkExists(roomOptionsPath)) {
		loadRoomOptions();
	}
}

void Room::enterJail(const string& runAsUser)
{
	SetuidHelper::raisePrivileges();

#if __FreeBSD__
	int jid = jail_getid(jailName.c_str());
	if (jid < 0) {
		throw std::runtime_error("unable to get jail ID");
	}

	//SetuidHelper::logPrivileges();

	if (jail_attach(jid) < 0) {
		log_errno("jail_attach(2) to jid %d", jid);
		throw std::system_error(errno, std::system_category());
	}
#else
	if (chdir(chrootDir.c_str()) < 0) {
		log_errno("chdir(2) to `%s'", chrootDir.c_str());
		throw std::system_error(errno, std::system_category());
	}

	if (chroot(chrootDir.c_str()) < 0) {
		log_errno("chroot(2) to `%s'", chrootDir.c_str());
		throw std::system_error(errno, std::system_category());
	}
#endif

	//FIXME: hardcoded; should consult PasswdEntry instead
	string homedir = "/usr/home/" + ownerLogin;
	if (chdir(homedir.c_str()) < 0) {
		log_errno("chdir(2) to %s", homedir.c_str());
		//throw std::system_error(errno, std::system_category());
	}

	if (runAsUser == ownerLogin) {
		SetuidHelper::dropPrivileges();
	} else {
		// XXX-FIXME: assumes root here
		log_debug("retaining root privs");
	}
}

void Room::exec(std::vector<std::string> execVec, const string& runAsUser)
{
	string loginName;
	string homeDir;
	if (runAsUser == "") {
		loginName = ownerLogin;
	} else {
		loginName = runAsUser;
	}
	if (loginName == "root") {
		homeDir = "/root";
	} else {
		homeDir = "/usr/home/" + runAsUser; //FIXME: hardcoded; should consult PasswdEntry instead"
	}

	char *path = NULL;
	int jid = jail_getid(jailName.c_str());
	if (jid < 0) {
		log_debug("jail `%s' not running; will start it now", jailName.c_str());
		start();
	}

	enterJail(loginName);

	string x11_display = "DISPLAY=";
	string x11_xauthority = "XAUTHORITY=";
	string dbus_address = "DBUS_SESSION_BUS_ADDRESS=";
	string jail_username = loginName;
	string env_username = "USER=" + loginName;
	string env_home = "HOME=" + homeDir;
	if (roomOptions.allowX11Clients) {
		if (getenv("DISPLAY")) x11_display += getenv("DISPLAY");
		if (getenv("XAUTHORITY")) x11_xauthority += getenv("XAUTHORITY");
		if (getenv("DBUS_SESSION_BUS_ADDRESS")) dbus_address += getenv("DBUS_SESSION_BUS_ADDRESS");
	}

	std::vector<char*> argsVec;
	if (execVec.size() == 0) {
		path = strdup("/bin/csh");
		argsVec.push_back((char*)"-csh");
	} else {
		path = strdup(execVec[0].c_str());
		for (string& s : execVec) {
			char *tmp = strdup(s.c_str());
			argsVec.push_back(tmp);
		}
	}
	argsVec.push_back(NULL);

	char* const envp[] = {
			(char*)env_home.c_str(),
			(char*)"SHELL=/bin/csh", //FIXME: should consult PasswdEntry
			(char*)"PATH=/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin",
			(char*)"TERM=xterm",
			(char*)env_username.c_str(),
			(char*)x11_display.c_str(),
			(char*)x11_xauthority.c_str(),
			(char*)dbus_address.c_str(),
			NULL
	};
	if (execve(path, argsVec.data(), envp) < 0) {
		log_errno("execve(2)");
		throw std::system_error(errno, std::system_category());
	}
	//NOTREACHED
	log_errno("execve(2)");
	throw std::runtime_error("exec failed");
}

void Room::enter() {
	std::vector<std::string> argsVec;
	Room::exec(argsVec, "");
}

bool Room::jailExists()
{
	int jid = jail_getid(jailName.c_str());
	log_debug("jail `%s' has jid %d", jailName.c_str(), jid);
	return (jid >= 0);
}

void Room::getJailName()
{
	jailName = "room_" + ownerLogin + "_";
	for (std::string::iterator it = roomName.begin(); it != roomName.end(); ++it) {
		if (*it == '.') {
			jailName.push_back('^');
		} else {
			jailName.push_back(*it);
		}
	}
}

void Room::validateName(const string& name)
{
	string buf = name;
	std::locale loc("C");

	roomName = name;

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
			continue;
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
			zpoolName + "/room/" + ownerLogin + "/" + destRoom
	});

	Room cloneRoom(roomDir, destRoom);
	SetuidHelper::lowerPrivileges();

	// Assume that when cloning a template, we don't want the new room
	// to be hidden.
	cloneRoom.getRoomOptions().isHidden = false;
	cloneRoom.syncRoomOptions();

	log_debug("clone complete");
}

void Room::create(const string& baseTarball)
{
	string cmd;

	SetuidHelper::raisePrivileges();

	log_debug("creating room");

	Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName });

	Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName + "/share"});
	FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/etc", 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/local", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/home", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/tmp", 0700, ownerUid, ownerGid);

	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });

	Shell::execute("/sbin/mount", {
			"-t", "devfs", "-o", "ruleset=4", "devfs", chrootDir + "/dev",
	});

	pushResolvConf();

	// KLUDGE: install pkg(8)
	int rv;
	Shell::execute("/usr/sbin/chroot", {
			"-u", "root",
			chrootDir,
			"env", "ASSUME_ALWAYS_YES=YES", "pkg", "bootstrap"
	}, rv);

	Shell::execute("/sbin/umount", { chrootDir + "/dev" });

	if (rv != 0) {
		throw std::runtime_error("failed to bootstrap pkg");
	}

	SetuidHelper::lowerPrivileges();

	syncRoomOptions();

	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", {
			"snapshot",
			roomDataset + "/" + roomName + "@__initial"
	});
	SetuidHelper::lowerPrivileges();

	log_debug("room %s created", roomName.c_str());
}

void Room::syncRoomOptions()
{
	SetuidHelper::raisePrivileges();

	// FIXME: this really should not be done as root

	roomOptions.save(roomOptionsPath);
	SetuidHelper::lowerPrivileges();
}

void Room::start() {
	int rv;

	if (jail_getid(jailName.c_str()) >= 0) {
		log_debug("room already started");
		return;
	}

	log_debug("booting room: %s", roomName.c_str());

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

	if (roomOptions.shareTempDir) {
		log_debug("using a shared /tmp");
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/tmp", chrootDir + "/tmp" });
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/var/tmp", chrootDir + "/var/tmp" });
	} else {
		log_debug("using a private /tmp");
	}

	if (roomOptions.shareHomeDir) {
		PasswdEntry pwent(ownerUid);
		Shell::execute("/sbin/mount", { "-t", "nullfs", pwent.getHome(), chrootDir + pwent.getHome() });
	}

	if (roomOptions.useLinuxABI) {
		// TODO: maybe load kernel modules? or maybe require that be done during system boot...
		//       requires:    kldload linux fdescfs linprocfs linsysfs tmpfs
		Shell::execute("/sbin/mount", { "-t", "linprocfs", "linprocfs", chrootDir + "/proc" });
		Shell::execute("/sbin/mount", { "-t", "linsysfs", "linsysfs", chrootDir + "/sys" });
	}

	try {
		log_debug("updating /etc/passwd");
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
	} catch(...) {
		log_warning("unable to create user account");
	}

	pushResolvConf();

	SetuidHelper::lowerPrivileges();
}

void Room::stop()
{
	string cmd;

	log_debug("stopping room `%s'", roomName.c_str());

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

	if (true || roomOptions.shareTempDir) {
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

	if (roomOptions.useLinuxABI) {
		Shell::execute("/sbin/umount", { chrootDir + "/proc" });
		Shell::execute("/sbin/umount", { chrootDir + "/sys" });
	}

	if (roomOptions.shareHomeDir) {
		if (roomOptions.shareHomeDir) {
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


	try {
		PasswdEntry pwent(ownerUid);
		Shell::execute("/usr/sbin/chroot", {
				chrootDir,
				"/usr/sbin/pw", "user", "del", "-u", std::to_string(ownerUid),
		});
	} catch (...) {
		log_warning("unable to delete user account");
	}

	// KLUDGE: See issue #13 for a better idea.
	Shell::execute("/usr/sbin/chroot", {
			chrootDir,
			"rm", "-f", "/etc/resolv.conf"
	});

	SetuidHelper::lowerPrivileges();

	log_notice("room `%s' has been stopped", roomName.c_str());
}

void Room::destroy()
{
	string cmd;

	log_debug("destroying room at %s", chrootDir.c_str());

	if (jail_getid(jailName.c_str()) >= 0) {
		stop();
	}

	SetuidHelper::raisePrivileges();
	if (useZfs) {
		// this races with "jail -r"
		bool success = false;
		for (int i = 0; i < 5; i++) {
			sleep(1);
			int result;
			Shell::execute("/sbin/zfs", { "destroy", "-r", roomDataset + "/" + roomName}, result);
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

	log_notice("room has been destroyed");

	SetuidHelper::lowerPrivileges();
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

void Room::install(const struct RoomInstallParams& rip)
{
	string tarball = "/tmp/base.txz"; //XXX-FIXME insecure tempdir use

	if (!FileUtil::checkExists(tarball)) {
		Shell::execute("/usr/bin/fetch", {
				"-q",
				"-o", tarball,
				rip.baseArchiveUri,
		});
	}

	Room room(rip.roomDir, rip.name);
	room.roomOptions = rip.options;
	room.create(tarball);

	room.syncRoomOptions();

	// FIXME: disabled for testing
#if 0
	if (unlink(tarball.c_str()) < 0) {
        log_errno("unlink(2)");
        throw std::system_error(errno, std::system_category());
	}
#endif
}

void Room::pushResolvConf()
{
	// KLUDGE: copy /etc/resolv.conf. See issue #13 for a better idea.
	Shell::execute("/bin/sh", {
			"-c",
			"cat /etc/resolv.conf | chroot " + chrootDir + " dd of=/etc/resolv.conf status=none"
	});
}
