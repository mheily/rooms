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

static FILE *logfile;
#include "logger.h"
}

using std::cout;
using std::cin;
using std::endl;
using std::string;

class Shell {
public:
	static string popen_readline(const string& command);

	static int executeWithStatus(const string& command) {
		log_debug("running %s", command.c_str());
		int status = system(command.c_str());
		if (status < 0) {
			log_error("command failed: %s", command.c_str());
			throw std::runtime_error("command failed");
		}

		return WEXITSTATUS(status);
	}

	static void execute(const string& command) {
		int status = executeWithStatus(command);
		if (status != 0) {
			throw std::runtime_error("command returned unexpected status code " +
					std::to_string(status) + ": " + command);
		}
	}
};

// Run a command that is expected to return a single line of output
// Return the line, without the trailing newline
string Shell::popen_readline(const string& command)
{
	FILE *in;
	char buf[4096];

	in = popen(command.c_str(), "r");
	if (!in) {
		throw std::runtime_error("popen failed");
	}
	if (fgets(buf, sizeof(buf), in) == NULL) {
		throw std::runtime_error("empty response");
	}
	if (ferror(in)) {
		throw std::runtime_error("ferror");
	}
	if (feof(in) != 0) {
		throw std::runtime_error("buffer too small");
	}

	string s = buf;
	if (!s.empty() && s[s.length() - 1] == '\n') {
		s.erase(s.length() - 1);
	}

	return s;
}

class FileUtils {
public:
	static bool checkExists(const string& path) {
		if (eaccess(path.c_str(), F_OK) == 0) {
			return true;
		} else {
			if (errno != ENOENT) {
				log_errno("eaccess(2) of `%s'", path.c_str());
				throw std::system_error(errno, std::system_category());
			}
			return false;
		}
	}

	static void mkdir_idempotent(const string& path, mode_t mode, uid_t uid, gid_t gid) {
		if (mkdir(path.c_str(), mode) != 0) {
			if (errno == EEXIST) {
				return;
			}
			log_errno("mkdir(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}

		if (chown(path.c_str(), uid, gid) != 0) {
			(void) unlink(path.c_str());
			log_errno("chown(2) of `%s'", path.c_str());
			throw std::system_error(errno, std::system_category());
		}
	}
};

class Room {
public:
	Room(const string& managerRoomDir, const string& name, uid_t uid);

	void create(const string& baseTarball);
	void killAllProcesses();
	void destroy();
	void enter();

private:
	char ownerPwEntBuf[9999]; // storage used by getpwuid_r(3)
	struct passwd ownerPwEnt; // the owner's passwd(5) entry
	uid_t  ownerUid;  // the UID who owns the room
	string roomDir;   // copy of RoomManager::roomDir
	string chrootDir; // path to the root of the chroot environment
	string roomName;  // name of this room
	string jailName;  // name of the jail
	bool allowX11Clients = true; // allow X programs to run
	bool shareTempDir = true; // share /tmp with the main system, sadly needed for X11 and other progs

	void getPasswdInfo(uid_t uid);
	bool jailExists();
	void customizeWithoutRoot();
	void enableX11Clients();
	void validateName(const string& name);
};

Room::Room(const string& managerRoomDir, const string& name, uid_t uid)
{
	getPasswdInfo(uid);
	validateName(name);

	roomDir = managerRoomDir;
	ownerUid = uid;
	chrootDir = roomDir + "/" + std::to_string(ownerUid) + "/" + name;
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
		jail_username = string(ownerPwEnt.pw_name);
		env_username = "USER=" + string(ownerPwEnt.pw_name);
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

	roomName = name;
	jailName = "room_" + string(ownerPwEnt.pw_name) + "_";

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

void Room::getPasswdInfo(uid_t uid)
{
	struct passwd *result;

	if (getpwuid_r(uid, &ownerPwEnt, (char*)&ownerPwEntBuf,
			sizeof(ownerPwEntBuf), &result) != 0) {
		log_errno("getpwuid_r(3)");
		throw std::system_error(errno, std::system_category());
	}
	if (result == NULL) {
		throw std::runtime_error("missing passwd entry for UID " + std::to_string(uid));
	}
}

void Room::enableX11Clients() {
	Shell::execute("cp /var/tmp/.PCDMAuth-* " + chrootDir + "/var/tmp");

	// FIXME: SECURITY
	// Despite having the cookie, clients fail with 'No protocol specified'
	// This workaround is bad for security in a multi-user system, but gets things going for a desktop.
	Shell::execute("xhost +local >/dev/null");
}

void Room::create(const string& baseTarball)
{
	string cmd;

	// Each user has a directory under /var/room
	FileUtils::mkdir_idempotent(string(roomDir + '/' + std::to_string(ownerUid)), 0700, ownerUid, (gid_t) ownerUid);

	log_debug("creating room");
	FileUtils::mkdir_idempotent(chrootDir, 0700, ownerUid, (gid_t) ownerUid);

	Shell::execute("tar -C " + chrootDir + " -xf " + baseTarball);

	Shell::execute(string("pw -R " + chrootDir + " user add -u " + std::to_string(ownerUid) +
			" -n " + string(ownerPwEnt.pw_name) +
			" -c " + string(ownerPwEnt.pw_gecos) +
			" -s " + string(ownerPwEnt.pw_shell) +
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

	if (!FileUtils::checkExists(chrootDir)) {
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

	// remove all files (this makes me nervous)
	Shell::execute("rm -rf " + chrootDir);

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
	return FileUtils::checkExists("/room");
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
	FileUtils::mkdir_idempotent(roomDir, 0700, 0, 0);
}

void RoomManager::downloadBase() {
	if (!FileUtils::checkExists(baseTarball)) {
		cout << "Downloading base.txz..\n";
		string cmd = "fetch -o " + baseTarball + " " + baseUri;
		if (system(cmd.c_str()) != 0) {
			throw std::runtime_error("Download failed");
		}
	}
}

string RoomManager::getZfsPoolName(const string& path)
{
	if (!FileUtils::checkExists(path)) {
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
