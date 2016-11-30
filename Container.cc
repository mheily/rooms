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

#include <string>
#include <iostream>
#include <fstream>

#ifdef __linux__
#include "LinuxJail.hpp"
#endif

#ifdef __FreeBSD__
#include "FreeBSDJail.hpp"
#endif

#include <csignal>

#include "fileUtil.h"
#include "logger.h"
#include "MountUtil.hpp"

Container* Container::create(const std::string& chrootDir)
{
	Container* c;
#ifdef __linux__
	c = new LinuxJail;
#elif __FreeBSD__
	c = new FreeBSDJail;
#else
#error Unsupported OS
#endif
	c->chrootDir = chrootDir;
	return (c);
}

void Container::runMainHook()
{
#ifdef __linux__
	LinuxJail::main_hook();
#elif __FreeBSD__
	//FreeBSDJail::main_hook();
#else
#error Unsupported OS
#endif
}

void Container::killInit()
{
	pid_t pid;

	if (!FileUtil::checkExists(initPidfilePath)) {
		log_warning("unable to kill init; pidfile does not exist");
		return;
	}

	pid = getInitPid();
	FileUtil::unlink(initPidfilePath);	
	SetuidHelper::raisePrivileges();
	log_debug("sending SIGTERM to pid %zu", (unsigned long)pid);
	if (kill(pid, SIGTERM) < 0) {
		err(1, "kill of %zu", (unsigned long) pid);
	}
	SetuidHelper::lowerPrivileges();
}

pid_t Container::getInitPid()
{
	std::string pidstr;
	std::ifstream pidfile;
	pidfile.open (initPidfilePath);
	pidfile >> pidstr;
	pidfile.close();
	return (std::atoi(pidstr.c_str()));
}

bool Container::checkInitPidValid()
{
	pid_t pid = getInitPid();
	if (kill(pid, 0) < 0) {
		if (errno == ESRCH) {
			return false;
		} else {
			err(1, "kill of %zu", (unsigned long) pid);
		}
	}
	// TODO: need to also verify this is a legit room init process,
	// and not just a recycled PID from a previous boot.
	return true;
}

void Container::mkdir_p(const string& path, mode_t mode, uid_t uid, gid_t gid)
{ 
	pid_t pid = fork();

	if (pid < 0)
		err(1, "fork()");

	if (pid > 0) {
		int status;
		wait(&status);
		if (!WIFEXITED(status) || WEXITSTATUS(status)) 
			errx(1, "mkdir_p of %s failed with status %d", path.c_str(), WEXITSTATUS(status));
	} else {

		SetuidHelper::raisePrivileges();
		if (chroot(chrootDir.c_str()) < 0)
			err(1, "chroot(2) to %s", chrootDir.c_str());

		/* In the child */
		if (::mkdir(path.c_str(), mode) != 0) {
			if (errno == EEXIST) {
				exit(0);
			}
			err(1, "mkdir(2) of `%s'", path.c_str());
		}

		if (chown(path.c_str(), uid, gid) != 0) {
			(void) unlink(path.c_str());
			err(1, "chown(2) of `%s'", path.c_str());
		}
		SetuidHelper::lowerPrivileges();
		exit(0);
	}
}

void Container::mount_into(const string& src, const string& relativeTarget) 
{
	string target = chrootDir + "/" + relativeTarget;

	/* CAVEAT: The security of this check depends on
	   it being executed before any user-controlled	
	   processes have been started in the container.
		Otherwise, it is a classic TOCTOU race.

	   TODO: use a state machine to track the state of
		the container, and have this function
		verify it is in the state described above.
	*/
#if 0
// interacting badly w/ mount namespaces on Linux
	struct stat expected, actual;
	if (stat(chrootDir.c_str(), &expected) < 0)
		err(1, "stat(2) of %s", chrootDir.c_str());
	if (stat(target.c_str(), &actual) < 0)
		err(1, "stat(2) of %s", target.c_str());
	if (actual.st_dev != expected.st_dev) 
		errx(1, "target must be on the same device as the chroot");
#endif
	log_debug("mounting %s at %s", src.c_str(), target.c_str());
	SetuidHelper::raisePrivileges();
#ifdef __linux__
	if (::mount(src.c_str(), target.c_str(), NULL, MS_BIND, NULL) < 0) {
		err(1, "mount(2) of %s", src.c_str());
	}
#elif defined(__FreeBSD__)
	if (::mount("nullfs", target.c_str(), 0, (void*)src.c_str()) < 0) {
		err(1, "mount(2) of %s", src.c_str());
	}
#else
#error Unsupported OS
#endif
	SetuidHelper::lowerPrivileges();
}

void Container::unmount(const string& relativePath) {
	string path = chrootDir + relativePath;

	log_debug("unmounting %s", path.c_str());
	SetuidHelper::raisePrivileges();
#ifdef __linux__
	if (::umount2(path.c_str(), 0) < 0) {
#else
	if (::unmount(path.c_str(), 0) < 0) {
#endif
		err(1, "unmount(2) of `%s'", path.c_str());
	}
	SetuidHelper::lowerPrivileges();
}

void Container::unmount_idempotent(const string& relativePath) {
	if (MountUtil::checkIsMounted(string(chrootDir + relativePath))) {
		unmount(relativePath);
	}
}
