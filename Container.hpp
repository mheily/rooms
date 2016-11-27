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

#pragma once

#include <string>

#include <sys/types.h>

#include "setuidHelper.h"

class Container {
public:
	virtual bool isRunning() = 0;
	virtual void enter() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void unmountAll() = 0;
	virtual void mountAll() = 0;
	static void runMainHook();
	static Container* create(const std::string& chrootDir);

	void setInitPidfilePath(const std::string& initPidfilePath) {
		this->initPidfilePath = initPidfilePath;
	}

	void setHostname(const std::string& hostname) {
		// TODO: validation
		this->hostname = hostname;
	}

	void unmount(const std::string& path);
	void unmount_idempotent(const std::string& path);
	void mount_into(const std::string& src, const std::string& relativeTarget);
	void mkdir_p(const std::string& path, mode_t mode, uid_t uid, gid_t gid);

	pid_t getInitPid();
	bool checkInitPidValid();
	void killInit();

	std::string chrootDir;

	// path to the PID file for the init(1) process inside
	// the container
	std::string initPidfilePath;

	// PID of the init(1) process
	pid_t initPid = 0;

//TODO: once getters are created: 
//private:
	std::string hostname;

};
