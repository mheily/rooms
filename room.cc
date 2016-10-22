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
#include "UuidGenerator.hpp"

extern char **environ;

Room::Room(const string& managerRoomDir, const string& name)
{
	roomDir = managerRoomDir;
	ownerUid = SetuidHelper::getActualUid();
	ownerGid = (gid_t) ownerUid; // FIXME: bad assumption

	PasswdEntry pwent(ownerUid);
	ownerLogin = pwent.getLogin();

	validateName(name);
	roomName = name;
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

	PasswdEntry pwent(ownerUid);
	if (chdir(pwent.getHome()) < 0) {
		log_errno("chdir(2) to %s", pwent.getHome());
		throw std::system_error(errno, std::system_category());
	}

	if (runAsUser == ownerLogin) {
		SetuidHelper::dropPrivileges();
	} else {
		// XXX-FIXME: assumes root here
		log_debug("retaining root privs");
	}
}

int Room::forkAndExec(std::vector<std::string> execVec, const string& runAsUser) {
	pid_t pid = fork();
	if (pid < 0) {
		log_errno("fork(2)");
		throw std::runtime_error("fork failed");
	}
	if (pid == 0) {
		exec(execVec, runAsUser);
		exit(1);
	}

	int status;
	if (waitpid(pid, &status, 0) < 0) {
		return -1;
	}
	if (!WIFEXITED(status)) {
		return -1;
	}
	int exitStatus = WEXITSTATUS(status);

	return exitStatus;
}

void Room::exec(std::vector<std::string> execVec, const string& runAsUser)
{
	PasswdEntry pwent(ownerUid);
	static char *clean_environment = NULL;
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
		homeDir = pwent.getHome();
	}

	char *path = NULL;
	int jid = jail_getid(jailName.c_str());
	if (jid < 0) {
		log_debug("jail `%s' not running; will start it now", jailName.c_str());
		start();
	}

	enterJail(loginName);

	string jail_username = loginName;

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

	char *env_display;
	char *env_xauthority;
	char *env_dbus;
	if (roomOptions.allowX11Clients) {
		env_display = getenv("DISPLAY");
		env_xauthority = getenv("XAUTHORITY");
		env_dbus = getenv("DBUS_SESSION_BUS_ADDRESS");
	}

	environ = &clean_environment;
	setenv("HOME", homeDir.c_str(), 1);
	setenv("SHELL", "/bin/csh", 1); //FIXME: should consult PasswdEntry
	setenv("PATH", "/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin", 1);
	setenv("TERM", "xterm", 1);
	setenv("USER", loginName.c_str(), 1);

	if (roomOptions.allowX11Clients) {
		if (env_display) setenv("DISPLAY", env_display, 1);
		if (env_xauthority) setenv("XAUTHORITY", env_xauthority, 1);
		if (env_dbus) setenv("DBUS_SESSION_BUS_ADDRESS", env_dbus, 1);
	}

	if (execvp(path, argsVec.data()) < 0) {
		log_errno("execvp(2)");
		throw std::system_error(errno, std::system_category());
	}
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

// SEE ALSO: cloneFromOrigin()
void Room::clone(const string& snapshot, const string& destRoom, const RoomOptions& roomOpt)
{
	log_debug("cloning room");

	Room cloneRoom(roomDir, destRoom);
	cloneRoom.createEmpty();

	// Replace the empty "share" dataset with a clone of the original
	string src = roomDataset + "/" + roomName + "/share@" + snapshot;
	string dest = zpoolName + "/room/" + ownerLogin + "/" + destRoom + "/share";
	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", { "destroy", dest });
	Shell::execute("/sbin/zfs", { "clone", src, dest });
	Shell::execute("/sbin/zfs", {"allow", "-u", ownerLogin, "hold,send", zpoolName + "/room/" + ownerLogin + "/" + destRoom });
	SetuidHelper::lowerPrivileges();

	// Copy the options.json file
	Shell::execute("/bin/cp", { roomOptionsPath, cloneRoom.roomOptionsPath});
	cloneRoom.loadRoomOptions();

	// Merge options from the CLI
	cloneRoom.roomOptions.merge(roomOpt);

	// Assume that newly cloned rooms should not be hidden.
	cloneRoom.getRoomOptions().isHidden = false;

	// Generate a new UUID
    UuidGenerator ug;
    ug.generate();
    cloneRoom.getRoomOptions().uuid = ug.getValue();

	cloneRoom.syncRoomOptions();
	cloneRoom.snapshotCreate(snapshot);

	log_debug("clone complete");
}

void Room::cloneFromOrigin(const string& uri)
{
	string scheme, host, path;

	Room::parseRemoteUri(uri, scheme, host, path);

	createEmpty();

	string tmpdir = roomDataDir + "/local/tmp";
	Shell::execute("/usr/bin/fetch", { "-q", "-o", roomOptionsPath, uri+"/options.json" });
	Shell::execute("/usr/bin/fetch", { "-q", "-o", tmpdir, uri+"/share.zfs.xz" });
	Shell::execute("/usr/bin/unxz", { tmpdir + "/share.zfs.xz" });

	// Replace the empty "share" dataset with a clone of the original
	string dataset = roomDataset + "/" + roomName + "/share";
	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", { "destroy", dataset });
	Shell::execute("/bin/sh", { "-c", "/sbin/zfs recv -F " + dataset + " < " + tmpdir + "/share.zfs" });
	Shell::execute("/sbin/zfs", {"allow", "-u", ownerLogin, "hold,send", zpoolName + "/room/" + ownerLogin + "/" + roomName });
	SetuidHelper::lowerPrivileges();

	log_debug("clone complete");
}

// Create an empty room, ready for share/ to be populated
void Room::createEmpty()
{
	log_debug("creating an empty room");

	// Generate a UUID
    UuidGenerator ug;
    ug.generate();
    roomOptions.uuid = ug.getValue();

	SetuidHelper::raisePrivileges();

	Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName });

	Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName + "/share"});

	// Allow the owner to use 'zfs send'
	Shell::execute("/sbin/zfs", {"allow", "-u", ownerLogin, "hold,send", roomDataset + "/" + roomName });

	FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/etc", 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/local", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/home", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/tmp", 0700, ownerUid, ownerGid);

	SetuidHelper::lowerPrivileges();
}

void Room::extractTarball(const string& baseTarball)
{
	string cmd;

	log_debug("creating room");

	syncRoomOptions();

	SetuidHelper::raisePrivileges();
	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });
	pushResolvConf();
	SetuidHelper::lowerPrivileges();

	log_debug("room %s created", roomName.c_str());
}

void Room::syncRoomOptions()
{
	roomOptions.save(roomOptionsPath);
}

void Room::send() {
	SetuidHelper::raisePrivileges();

	Subprocess proc;
	proc.execve("/sbin/zfs", { "send", "-R",
			string(roomDataset + "/" + roomName + "@" + getLatestSnapshot()),
	});
}

void Room::snapshotCreate(const string& name)
{
	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", {
		"snapshot", "-r",
		roomDataset + "/" + roomName + "@" + name,
	});
	SetuidHelper::lowerPrivileges();
}

void Room::snapshotDestroy(const string& name)
{
	SetuidHelper::raisePrivileges();
	Shell::execute("/sbin/zfs", {
		"destroy", "-r",
		roomDataset + "/" + roomName + "@" + name,
	});
	SetuidHelper::lowerPrivileges();
}

void Room::printSnapshotList()
{
	// FIXME: would like to avoid this popen, for better security
	string cmd = "zfs list -H -r -d 1 -t snapshot -o name -s creation " +
			roomDataset + "/" + roomName + "/share | sed 's/.*@//'";

	Subprocess proc;
	proc.execve("/bin/sh", { "-c", cmd });
}

void Room::start() {
	int rv;

	if (jail_getid(jailName.c_str()) >= 0) {
		log_debug("room already started");
		return;
	}

	PasswdEntry pwent(ownerUid);

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

	// Mount the /local directory as /data within the jail
	string data_dir = string(chrootDir + "/data");
	unlink(data_dir.c_str());
	mkdir(data_dir.c_str(), 0755);
	Shell::execute("/sbin/mount", { "-t", "nullfs",
			string(roomDataDir + "/local").c_str(),
			data_dir.c_str()
	});

	if (roomOptions.shareTempDir) {
		log_debug("using a shared /tmp");
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/tmp", chrootDir + "/tmp" });
		Shell::execute("/sbin/mount", { "-t", "nullfs", "/var/tmp", chrootDir + "/var/tmp" });
	} else {
		log_debug("using a private /tmp");
		Shell::execute("/sbin/mount", {
				"-t", "nullfs",
				roomDataDir + "/local/tmp",
				chrootDir + "/tmp" });
		Shell::execute("/sbin/mount", {
				"-t", "nullfs",
				roomDataDir + "/local/tmp",
				chrootDir + "/var/tmp" });
	}

	Shell::execute("/usr/sbin/chroot", {
		chrootDir,
		"mkdir", "-p", pwent.getHome(),
	});
	if (roomOptions.shareHomeDir) {
		log_debug("using a shared /home");
		Shell::execute("/sbin/mount", {
				"-t", "nullfs",
				pwent.getHome(),
				chrootDir + pwent.getHome() });
	} else {
		log_debug("using a private /home");
		Shell::execute("/sbin/mount", {
				"-t", "nullfs",
				roomDataDir + "/local/home",
				chrootDir + pwent.getHome() });
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
	PasswdEntry pwent(ownerUid);
	string cmd;

	int unmount_flags = 0; // Future: might support MNT_FORCE

	log_debug("stopping room `%s'", roomName.c_str());

	SetuidHelper::raisePrivileges();

	if (chrootDir == "" || chrootDir == "/") {
		throw std::runtime_error("bad chrootDir");
	}

	if (!FileUtil::checkExists(chrootDir)) {
		throw std::runtime_error("room does not exist");
	}

	killAllProcesses();

	log_debug("unmounting /dev");
	if (unmount(string(chrootDir + "/dev").c_str(), unmount_flags) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
		}
	}

	if (true || roomOptions.shareTempDir) {
		log_debug("unmounting /tmp");
		if (unmount(string(chrootDir + "/tmp").c_str(), unmount_flags) < 0) {
			if (errno != EINVAL) {
				log_errno("unmount(2)");
				throw std::system_error(errno, std::system_category());
			}
		}
		log_debug("unmounting /var/tmp");
		if (unmount(string(chrootDir + "/var/tmp").c_str(), unmount_flags) < 0) {
			if (errno != EINVAL) {
				log_errno("unmount(2)");
				throw std::system_error(errno, std::system_category());
			}
		}
	}

	log_debug("unmounting /home");
	if (unmount(string(chrootDir + pwent.getHome()).c_str(), unmount_flags) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
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

	log_debug("unmounting /data");
	if (unmount(string(chrootDir + "/data").c_str(), unmount_flags) < 0) {
		if (errno != EINVAL) {
			log_errno("unmount(2)");
			throw std::system_error(errno, std::system_category());
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

		log_debug("unmounting root filesystem");
		FileUtil::unmount(roomDataDir + "/share");
		FileUtil::unmount(roomDataDir);

		// this used to race with "jail -r", but doesn't anymore
		bool success = false;
		for (int i = 0; i < 2; i++) {
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
	}

	log_notice("room has been destroyed");

	SetuidHelper::lowerPrivileges();
}

// Must be called w/ elevated privileges
void Room::killAllProcesses()
{
	log_debug("killing all processes for room %s", roomName.c_str());

	// TODO: have a "nice" option that uses SIGTERM as well

	int jid = jail_getid(jailName.c_str());

	if (jid < 0) {
		log_debug("jail does not exist; skipping");
		return;
	}

	if (jail_remove(jid) < 0) {
		log_errno("jail_remove(2)");
        throw std::system_error(errno, std::system_category());
	}
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

void Room::setOriginUri(const string& uri)
{
	// Validate syntax
	string scheme, host, path;
	Room::parseRemoteUri(uri, scheme, host, path);

	roomOptions.originUri = uri;
	syncRoomOptions();
}

void Room::downloadTarball(const string& uri, const string& path)
{
	string tarball = roomDataDir + "/base.txz";
	log_debug("downloading %s", uri.c_str());
	Shell::execute("/usr/bin/fetch", { "-q", "-o", path, uri });
}

void Room::installFromArchive(const string& uri)
{
	string tarball = roomDataDir + "/local/tmp/base.txz";
	downloadTarball(uri, tarball);
	extractTarball(tarball);
	FileUtil::unlink(tarball);
}

void Room::install(const struct RoomInstallParams& rip)
{
	Room room(rip.roomDir, rip.name);
	room.roomOptions = rip.options;
	room.createEmpty();
	room.syncRoomOptions();
	room.installFromArchive(rip.baseArchiveUri);
}

void Room::editConfiguration()
{
	SetuidHelper::raisePrivileges();
	SetuidHelper::dropPrivileges();
	char* editor = getenv("EDITOR");
	if (!editor) {
		editor = strdup("vi");
	}
	string options_file = roomDataDir + "/etc/options.json";
	char *oarg = strdup(options_file.c_str());
	char *args[] = { editor, oarg, NULL };
	if (execvp(editor, args) < 0) {
		cout << editor << " " << options_file;
		throw std::system_error(errno, std::system_category());
	}
}

void Room::pushResolvConf()
{
	// KLUDGE: copy /etc/resolv.conf. See issue #13 for a better idea.
	Shell::execute("/bin/sh", {
			"-c",
			"cat /etc/resolv.conf | chroot " + chrootDir + " dd of=/etc/resolv.conf status=none"
	});
}

// Generate a reasonably unique and meaningful ZFS snapshot name.
string Room::generateSnapshotName()
{
	const char *format = "%Y-%m-%d.%H:%M:%S";
	char buf[100];

	std::time_t t = std::time(NULL);
	if (!std::strftime(buf, sizeof(buf), format, std::localtime(&t))) {
		throw std::runtime_error("strftime failed");
	}
	return string(buf) + "_P" + std::to_string(getpid());
}

string Room::getLatestSnapshot()
{
	// FIXME: would like to avoid this popen, for better security
	string cmd = "zfs list -H -r -d 1 -t snapshot -o name -s creation " +
			roomDataset + "/" + roomName + "/share | tail -1 | sed 's/.*@//'";

	return Shell::popen_readline(cmd);
}

void Room::pushToOrigin()
{
	if (roomOptions.originUri == "") {
		throw std::runtime_error("origin URI cannot be empty");
	}

	log_debug("pushing room to origin %s", roomOptions.originUri.c_str());

	string scheme, host, path;
	Room::parseRemoteUri(roomOptions.originUri, scheme, host, path);

	log_debug("creating room directory on origin server");
    Subprocess p;
    p.setPreserveEnvironment(true);
    p.setDropPrivileges(true);
    p.execute("/usr/bin/ssh", { host, "mkdir", "-p", path + "/" + roomName });
	int result = p.waitForExit();
	if (result != 0) {
		throw std::runtime_error("mkdir failed");
	}

	log_debug("sending a replication stream package");
	{
		string snapPath = roomDataset + "/" + roomName + "/share@" + getLatestSnapshot();
		Subprocess p;
	    p.setPreserveEnvironment(true);
	    p.setDropPrivileges(true);
		p.execute("/bin/sh", { "-c", "/sbin/zfs send -R " + snapPath + " | xz | " +
			"ssh " + host + " 'cat > " + path + "/" + roomName + "/share.zfs.xz'" });
		int result = p.waitForExit();
		if (result != 0) {
			throw std::runtime_error("zfs send failed");
		}
	}

	log_debug("sending the room configuration file");
	{
		Subprocess p;
	    p.setPreserveEnvironment(true);
	    p.setDropPrivileges(true);
		p.execute("/usr/bin/scp", { "-q", roomOptionsPath, host + ":" + path + "/" + roomName });
		int result = p.waitForExit();
		if (result != 0) {
			throw std::runtime_error("scp failed");
		}
	}
}

void Room::parseRemoteUri(const string& uri, string& scheme, string& host, string& path)
{
	string buf = uri;
	size_t schemelen;

	if (buf.find("ssh://") == 0) {
		schemelen = 6;
		scheme = "ssh";
	} else if (buf.find("http://") == 0) {
		schemelen = 7;
		scheme = "http";
	} else {
		throw std::runtime_error("invalid URI");
	}

	buf = buf.substr(schemelen, string::npos);
	host = buf.substr(0, buf.find('/', schemelen));
	//cout << "host: " + host + "\n";
	path = buf.substr(host.length(), string::npos);
	//	cout << "path: " + path + "\n";
}
