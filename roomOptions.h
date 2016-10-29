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

#include "namespaceImport.h"

// Options that can be controlled by the user
struct RoomOptions {

	// If true, the room will be hidden from the output of "room list"
	bool isHidden = false;

	// allow X programs to run
	bool allowX11Clients = false;

	// share /tmp and /var/tmp with the main system, sadly needed for X11 and other progs
	bool shareTempDir = false;

	// share $HOME with the main system
	bool shareHomeDir = false;

	// what Kernel ABI the room uses (e.g. Linux, FreeBSD)
	string kernelABI;

	// Specify the minimum kernel version that the host must have
	// based on the ABI of the programs inside the room.
	// e.g. "Linux 2.6.32" or "FreeBSD 11.0-RELEASE-p1"
	// TODO: string kernelVersion;

	// The URI of the room that this room was cloned from
	string cloneUri;

	// The UUID of the room
	string uuid;

	// The remote "origin" of the room; similar to Git, this allows
	// you to push/pull changes to/from a remote server
	string originUri;

	void load(const string& path);
	void save(const string& path);
	void dump();
	void merge(const struct RoomOptions& src);
};
