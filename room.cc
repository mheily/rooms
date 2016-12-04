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

#include "jail_getid.h"

extern "C" {
#include <err.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
}

#include "namespaceImport.h"
#include "Container.hpp"
#include "shell.h"
#include "fileUtil.h"
#include "jail_getid.h"
#include "passwdEntry.h"
#include "room.h"
#include "setuidHelper.h"
#include "zfsDataset.h"
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
	useZfs = false; //TODO: change to ZfsPool::detectZfs();
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
	container = Container::create(chrootDir); //XXX-FIXME: WILL LEAK, NEED UNIQUE_PTR
	container->setInitPidfilePath(roomDataDir + "/etc/init.pid"); // TODO: move to a /var/run directory instead
	container->setHostname(roomName + ".room");
}

void Room::enterJail(const string& runAsUser)
{
	PasswdEntry pwent(ownerUid);

	SetuidHelper::raisePrivileges();

	container->enter();

	if (runAsUser == ownerLogin) {
		if (chdir(pwent.getHome()) < 0) {
			log_errno("ERROR: unable to chdir(2) to %s", pwent.getHome());
		}

#ifdef __linux__
		if (setgid(ownerGid) < 0) err(1, "setgid");
		if (setuid(ownerUid) < 0) err(1, "setuid");
#else
		SetuidHelper::dropPrivileges();
#endif
	} else {
		// XXX-FIXME: assumes root here
		log_debug("retaining root privs");
#ifdef __linux__
		if (setgid(0) < 0) err(1, "setgid");
		if (setuid(0) < 0) err(1, "setuid");
#endif
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
	char *path = NULL;

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

	if (!container->isRunning()) {
		log_debug("container `%s' not running; will start it now", jailName.c_str());
		container->start();
	}

	enterJail(loginName);

	pid_t pid = fork();

	if (pid < 0) err(1, "fork(2)");
	if (pid == 0) {

	string jail_username = loginName;

#if defined(__linux__)
	string login_shell = "/bin/bash";
	string login_shell_proctitle = "bash";
#elif defined(__FreeBSD__)
	string login_shell = "/bin/csh";
	string login_shell_proctitle = "-csh";
#else
	string login_shell = "/bin/sh";
	string login_shell_proctitle = "sh";
#endif

	std::vector<char*> argsVec;
	if (execVec.size() == 0) {
		path = strdup(login_shell.c_str());
		argsVec.push_back(strdup(login_shell_proctitle.c_str()));
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
	setenv("SHELL", login_shell.c_str(), 1); //FIXME: should consult PasswdEntry
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
	} else {
	//parent
		int status;
		wait(&status);
		exit(0);
	}
}

void Room::enter() {
	std::vector<std::string> argsVec;
	Room::exec(argsVec, ownerLogin);
}

bool Room::jailExists()
{
#ifdef __FreeBSD__
	int jid = jail_getid(jailName.c_str());
	log_debug("jail `%s' has jid %d", jailName.c_str(), jid);
	return (jid >= 0);
#else
	return (false); //XXX -- not sure if linux can do this
#endif
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

	auto new_options = cloneRoom.getRoomOptions();

	// Assume that newly cloned rooms should not be hidden.
	new_options.isHidden = false;

#if 0
    //DEADWOOD -- handled in Ruby now

	// Generate a new UUID
    UuidGenerator ug;
    ug.generate();
    new_options.uuid = ug.getValue();

    // Update the origin/clone URIs
	new_options.templateUri = roomOptions.originUri;
	new_options.originUri = "";
	new_options.templateSnapshot = snapshot;
#endif

	cloneRoom.syncRoomOptions();
	//cloneRoom.snapshotCreate(snapshot);
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

	if (useZfs) {
		Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName });

		Shell::execute("/sbin/zfs", {"create", roomDataset + "/" + roomName + "/share"});

		// Allow the owner to use 'zfs send'
		Shell::execute("/sbin/zfs", {"allow", "-u", ownerLogin, "hold,send", roomDataset + "/" + roomName });
	} else {
		FileUtil::mkdir_idempotent(roomDataDir, 0700, ownerUid, ownerGid);
		FileUtil::mkdir_idempotent(std::string(roomDataDir + "/share"), 0700, ownerUid, ownerGid);
	}

	FileUtil::mkdir_idempotent(chrootDir, 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/etc", 0700, ownerUid, ownerGid);

	FileUtil::mkdir_idempotent(roomDataDir + "/local", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/home", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/local/tmp", 0700, ownerUid, ownerGid);
	FileUtil::mkdir_idempotent(roomDataDir + "/tags", 0700, ownerUid, ownerGid);

	SetuidHelper::lowerPrivileges();
}

void Room::extractTarball(const string& baseTarball)
{
	string cmd;

	log_debug("creating room");

	syncRoomOptions();

	string tarCommand;
	if (FileUtil::checkExists("/bin/tar")) {
		tarCommand = "/bin/tar";
	} else if (FileUtil::checkExists("/usr/bin/tar")) {
		tarCommand = "/usr/bin/tar";
	} else {
		log_error("unable to find tar(1)");
		abort();
	}

	container->unpack(baseTarball);

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

// TODO: Move this into ZfsDataset::
void Room::promoteClone(const string& cloneDataset) {
	string msg;

	// FIXME: does not like full dataset paths like tank/foo/whatever
/*	if (!ZfsDataset::validateName(cloneDataset, msg)) {
		log_error("bad name: %s", msg.c_str());
		throw std::runtime_error("bad dataset name");
	}*/

	// XXX-SECURITY need to verify dataset is mounted at /room/$USER; otherwise this allows
	// promoting arbitrary datasets, such as /var

//	SetuidHelper::raisePrivileges();
//	Shell::execute("/sbin/zfs", { "promote", cloneDataset });
	throw; // DEADWOOD
}

void Room::snapshotReceive(const string& name)
{
	Subprocess proc;

	SetuidHelper::raisePrivileges();
	proc.execve("/sbin/zfs", { "receive", "-Fv",
			roomDataset + "/" + roomName, "/share",
	});
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
	if (container->isRunning()) {
		log_debug("room already started");
		return;
	}


	log_debug("booting room: %s", roomName.c_str());

#ifdef __FreeBSD
	container->jailName = jailName;
	if (roomOptions.kernelABI == "Linux") {
		// TODO: maybe load kernel modules? or maybe require that be done during system boot...
		//       requires:    kldload linux fdescfs linprocfs linsysfs tmpfs
		Shell::execute("/sbin/mount", { "-t", "linprocfs", "linprocfs", chrootDir + "/proc" });
		Shell::execute("/sbin/mount", { "-t", "linsysfs", "linsysfs", chrootDir + "/sys" });
	}

	//XXX-SECURITY need to do this after chroot
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
#else
	//XXX-SECURITY need to do this after chroot
	try {
		log_debug("updating /etc/passwd");
		PasswdEntry pwent(ownerUid);
		Shell::execute("/usr/sbin/useradd", {
				"-R",  chrootDir,
				"-u", std::to_string(ownerUid),
//				"-g", std::to_string(ownerGid),
				"-c", pwent.getGecos(),
				"-s", pwent.getShell(),
				"-m",
				pwent.getLogin(),
		});
	} catch(...) {
		log_warning("unable to create user account");
	}
#endif

	pushResolvConf();

	container->start();
}

void Room::stop()
{
	PasswdEntry pwent(ownerUid);
	string cmd;

#ifdef __linux__
	container->stop();
	return;
	// TODO: move code below into freebsdjail.cc
#endif

	log_debug("stopping room `%s'", roomName.c_str());

	SetuidHelper::raisePrivileges();

	if (chrootDir == "" || chrootDir == "/") {
		throw std::runtime_error("bad chrootDir");
	}

	if (!FileUtil::checkExists(chrootDir)) {
		throw std::runtime_error("room does not exist");
	}

	killAllProcesses();

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

void Room::destroy(const string& name)
{
	throw; //NOT IMPLEMENTED YET
}

void Room::destroy()
{
	string cmd;

	log_debug("destroying room at %s", chrootDir.c_str());

	if (JailUtil::isRunning(jailName)) {
		stop();
	}

	SetuidHelper::raisePrivileges();
	if (useZfs) {

		log_debug("unmounting root filesystem");
		FileUtil::unmount(roomDataDir + "/share", MNT_FORCE);
		FileUtil::unmount(roomDataDir, MNT_FORCE);

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
	} else {
#ifdef __FreeBSD__
		Shell::execute("/bin/chflags", { "-R", "noschg", roomDataDir });
#endif
		Shell::execute("/bin/rm", { "-rf", roomDataDir });
	}

	log_notice("room has been destroyed");

	SetuidHelper::lowerPrivileges();
}

void Room::mount() {
	container->mountAll();

	PasswdEntry pwent(ownerUid);
	container->mkdir_p("/data", 0755, 0, 0);
	container->mount_into(roomDataDir + "/local", "/data");

	if (roomOptions.shareTempDir) {
		log_debug("using a shared /tmp");
		container->mount_into("/tmp", "/tmp");
		container->mount_into("/var/tmp", "/var/tmp");
	} else {
		log_debug("using a private /tmp");
		container->mount_into(chrootDir + "/data/tmp", "/tmp");
		container->mount_into(chrootDir + "/data/tmp", "/var/tmp");
	}

	container->mkdir_p(pwent.getHome(), 0755, 0, 0);
	if (roomOptions.shareHomeDir) {
		log_debug("using a shared /home");
		container->mount_into(pwent.getHome(), pwent.getHome());
	} else {
		log_debug("using a private /home");
		container->mount_into(roomDataDir + "/local/home", pwent.getHome());
	}
}

void Room::unmount() {
	container->unmountAll();
	container->unmount_idempotent("/data");
	container->unmount_idempotent("/home");
	container->unmount_idempotent("/tmp");
	container->unmount_idempotent("/var/tmp");
}

// Must be called w/ elevated privileges
void Room::killAllProcesses()
{
	log_debug("killing all processes for room %s", roomName.c_str());

	// TODO: have a "nice" option that uses SIGTERM as well

	if (!JailUtil::isRunning(jailName)) {
		log_debug("jail does not exist; skipping");
		return;
	}

#ifdef __FreeBSD__
	int jid = jail_getid(jailName.c_str());
	if (jail_remove(jid) < 0) {
		log_errno("jail_remove(2)");
        throw std::system_error(errno, std::system_category());
	}
#else
	log_error("FIXME -- not implemented yet");
#endif
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

void Room::install(const struct RoomInstallParams& rip)
{
	Room room(rip.roomDir, rip.name);
	room.roomOptions = rip.options;
	room.createEmpty();
	room.syncRoomOptions();
	if (rip.baseArchiveUri != "") {
		room.extractTarball(rip.baseArchiveUri);
	} else {
		log_debug("created an empty room");
	}
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
	} else if (buf.find("file://") == 0) {
		schemelen = 7;
		scheme = "file";
	} else {
		throw std::runtime_error(__FILE__": invalid URI");
	}

	buf = buf.substr(schemelen, string::npos);
	host = buf.substr(0, buf.find('/', schemelen));
	//cout << "host: " + host + "\n";
	path = buf.substr(host.length(), string::npos);
	//	cout << "path: " + path + "\n";
}
