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

// RoomManager configuration options that can be controlled by a non-privileged user
struct RoomManagerUserOptions {

	// ---- BEGIN: Default security options when creating a room -----

	// allow X programs to run
	bool allowX11Clients = false;

	// share /tmp and /var/tmp with the main system, sadly needed for X11 and other progs
	bool shareTempDir = false;

	// share $HOME with the main system
	bool shareHomeDir = false;

	// ---- END: Default security options when creating a room -----

	// The name of the default room used when creating new rooms
	std::string defaultRoom;

	void load(const string& path);
	void save(const string& path);
	void dump();
};
