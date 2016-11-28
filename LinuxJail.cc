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


#include <iostream>
#include <fstream>

extern "C" {
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
}

#include "LinuxJail.hpp"
#include "MountUtil.hpp"
#include "fileUtil.h"
#include "logger.h"

static const size_t STACK_SIZE = (1024 * 1024);
static char jail_stack[STACK_SIZE];

/* Use pipe(2) to emulate a simple semaphore, so we don't have
   to link with -lpthread */
static int semfd[2];

static void update_map(pid_t pid, const char* file)
{
	std::string uid_map_path = "/proc/" + std::to_string(pid) + "/" + file;
	const char* mapbuf = "0 999999 60000\n";

	log_debug("updating %s", uid_map_path.c_str());
	int fd = open(uid_map_path.c_str(), O_RDWR);
	if (fd < 0) err(1, "open(2)");
	ssize_t bytes, len;
	len = strlen(mapbuf);
	bytes = write(fd, mapbuf, len);
	if (bytes < len) {
		err(1, "write(2) returned %d", (int) bytes);
	}
	if (close(fd) < 0) err(1, "close(2)");
}

static void enter_ns(pid_t pid, const char* nstype)
{
	std::string nsdir, nsfile;
	nsdir = "/proc/" + std::to_string(pid) + "/ns";

	nsfile = nsdir + "/" + nstype;
	log_debug("entering namespace %s", nsfile.c_str());
	int fd = open(nsfile.c_str(), O_RDONLY);
	if (fd < 0) err(1, "open(2) of %s", nsfile.c_str());
	if (setns(fd, 0) < 0) err(1, "setns(2)");
	close(fd);
}

static void initialize_uid_map(pid_t initPid)
{
	//std::ofstream uid_map;

//	pid_t pid = fork();
//	if (pid < 0) err(1, "fork");
//	if (pid > 0) {
	system("echo whee; id");
//	enter_ns(initPid, "pid");

	int fd = open(string("/proc/" + std::to_string(initPid) + "/setgroups").c_str(), O_RDWR);
	if (fd < 0) err(1, "open(2)");
	if (write(fd, "deny", 5) < 5) {
		err(1, "write(2)");
	}
	if (close(fd) < 0) err(1, "close(2)");

	update_map(initPid, "uid_map");
	update_map(initPid, "gid_map");
//		int status;
 //               wait(&status);
  //              if (!WIFEXITED(status) || WEXITSTATUS(status))
   //                     errx(1, "unable to update uid_map");
//		
//	} else {
//		sleep(5);
//	exit(0);
//	}
}

static int jailMain(void *arg)
{
	LinuxJail* jail = static_cast<LinuxJail*>(arg);

	//std::cout << "going to " << jail->chrootDir << "\n";
	//daemon(1, 0);

//        SetuidHelper::raisePrivileges();

	if (sethostname(jail->hostname.c_str(), jail->hostname.length()) < 0) {
		err(1, "sethostname(2)");
	}

	auto mountpoint = std::string(jail->chrootDir + "/proc");
	if (mount("proc", mountpoint.c_str(), "proc", 0, NULL) < 0) {
		err(1, "mount(2) of /proc");
	}

        if (chdir(jail->chrootDir.c_str()) < 0) {
		err(1, "chdir(2)");
        }

        if (chroot(jail->chrootDir.c_str()) < 0) {
		err(1, "chroot(2)");
        }

	// WORKAROUND: need a way to close inherited FDs more intelligently
	for (int i = 3; i < 30; i++) {
		(void) close(i);
	}

        //FIXME: WANT TO: SetuidHelper::dropPrivileges();

	// Wait for the termination signal
	sigset_t sigset;
	int sig;	
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGTERM);
	sigwait(&sigset, &sig);

	std::cout << "exiting init process\n";

	return (0);
}

LinuxJail::LinuxJail()
{
}

void LinuxJail::main_hook()
{
	if (prctl(PR_SET_KEEPCAPS, 1) < 0)
		err(1, "prctl(2)");
}

bool LinuxJail::isRunning()
{
        //SetuidHelper::raisePrivileges();
	//TODO
        //SetuidHelper::lowerPrivileges();

	if (FileUtil::checkExists(initPidfilePath)) {
		return checkInitPidValid();
	} else {
		return false;
	}
}

void LinuxJail::mountAll()
{
        SetuidHelper::raisePrivileges();

	auto mountpoint = std::string(chrootDir + "/sys");
	if (mount("/sys", mountpoint.c_str(), "", MS_BIND | MS_RDONLY, NULL) < 0) {
		err(1, "mount(2) of %s", mountpoint.c_str());
	}
	mountpoint = std::string(chrootDir + "/dev");
	if (mount("/dev", mountpoint.c_str(), "", MS_BIND, NULL) < 0) {
		err(1, "mount(2) of %s", mountpoint.c_str());
	}
	mountpoint = std::string(chrootDir + "/dev/pts");
	if (mount("devpts", mountpoint.c_str(), "devpts", 0, NULL) < 0) {
		err(1, "mount(2) of %s", mountpoint.c_str());
	}

        SetuidHelper::lowerPrivileges();
}

void LinuxJail::unmountAll()
{
        //SetuidHelper::raisePrivileges();
	auto uSys = MountUtil::checkIsMounted(std::string(chrootDir + "/sys"));
	auto uPts = MountUtil::checkIsMounted(std::string(chrootDir + "/dev/pts"));
	auto uDev = MountUtil::checkIsMounted(std::string(chrootDir + "/dev"));
        //SetuidHelper::lowerPrivileges();
	if (uSys)
		unmount("/sys");
	if (uPts)
		unmount("/dev/pts");
	if (uDev)
		unmount("/dev");


	// KLUDGE: would be better to keep a per-container fstab
	MountUtil::recursiveUnmount(chrootDir);
	
}

void LinuxJail::start()
{
	std::string mountpoint;

        SetuidHelper::raisePrivileges();

	if (pipe(semfd) < 0)
		err(1, "prctl(2)");

	int flags = CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWUTS;
	//std::cout << "about 2 going to " << chrootDir << "\n";
	initPid = clone(jailMain, jail_stack + STACK_SIZE, flags, this);
	if (initPid < 0) {
		err(1, "clone(2)");
	}

	initialize_uid_map(initPid);

	std::ofstream pidfile;
	pidfile.open (initPidfilePath);
	pidfile << std::to_string(initPid);
	pidfile.close();


        SetuidHelper::lowerPrivileges();

#if 0
//tries to keep a namespace alive
// pointless; we need an init(1) process in each container

	std::string nsdir, nsfile, target;
	nsdir = "/proc/" + std::to_string(pid) + "/ns";

	nsfile = nsdir + "/ipc";
	//target = jail->chrootDir + "/../../ns.ipc";
	target = "/tmp/ns.ipc";
	std::cout << "nsfile: " << nsfile << "\n";
	std::cout << "target: " << target << "\n";
	int fd = open(target.c_str(), O_CREAT | O_EXCL, 0600);
	if (fd < 0) err(1, "open(2)");
	close(fd);
	//if (mkdir(target.c_str(), 0700) < 0) err(1, "mkdir");
        SetuidHelper::raisePrivileges();
	if (mount(nsfile.c_str(), target.c_str(), NULL, MS_BIND, NULL) < 0) {
		err(1, "mount(2)");
	}
        SetuidHelper::lowerPrivileges();

#endif
}

void LinuxJail::stop()
{
	killInit();

	log_debug("stop complete");
}

void LinuxJail::enter()
{
	pid_t pid = getInitPid();

	log_debug("entering PID namespace %zu", (size_t) pid);
// FIXME: need this, but returns EPERM
//	update_map(getpid(), "uid_map");
	//update_map(getpid(), "gid_map");

	enter_ns(pid, "ipc");
	enter_ns(pid, "mnt");
	enter_ns(pid, "uts");
	enter_ns(pid, "pid");
#if 0
	log_error("FIXME - uid_map broken, container root is the real root user");
#else
	enter_ns(pid, "user");
#endif
	//not entered: net

        if (chdir(chrootDir.c_str()) < 0) {
		err(1, "chdir(2)");
        }

        if (chroot(chrootDir.c_str()) < 0) {
		err(1, "chroot(2)");
        }
}
