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

extern "C" {
#include <err.h>
#include <unistd.h>
}

#include "FreeBSDJail.hpp"
#include "jail_getid.h"

FreeBSDJail::FreeBSDJail()
{
}

bool FreeBSDJail::isRunning()
{
        if (jail_getid(jailName.c_str() < 0) {
		return (false);
        } else {
		return (false);
	}
}

void FreeBSDJail::enter()
{
        int jid = jail_getid(jailName.c_str());
        if (jid < 0) {
                throw std::runtime_error("unable to get jail ID");
        }

        if (jail_attach(jid) < 0) {
                log_errno("jail_attach(2) to jid %d", jid);
                throw std::system_error(errno, std::system_category());
        }
}

void FreeBSDJail::start()
{
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
}

void FreeBSDJail::stop()
{
}

void FreeBSDJail::unpack(const std::string& archivePath) 
{
	log_debug("unpacking %s", archivePath.c_str());
	SetuidHelper::raisePrivileges();
	Shell::execute("/usr/bin/tar", { "-C", chrootDir, "-xf", baseTarball });
	SetuidHelper::lowerPrivileges();
}
