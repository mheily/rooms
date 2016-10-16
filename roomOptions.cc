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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "roomOptions.h"
#include "logger.h"

namespace pt = boost::property_tree;

void RoomOptions::load(const string &path)
{
	pt::ptree tree;

    log_debug("reading options from %s", path.c_str());
    pt::read_json(path, tree);

	allowX11Clients = tree.get("permissions.allowX11Clients", false);
	shareTempDir = tree.get("permissions.shareTempDir", false);
	shareHomeDir = tree.get("permissions.shareHomeDir", false);
	useLinuxABI = tree.get("useLinuxABI", false);
	isHidden = tree.get("isHidden", false);
	cloneUri = tree.get("cloneUri", "");
	uuid = tree.get("uuid", "");
}


void RoomOptions::save(const string &path)
{
    pt::ptree tree;

    log_debug("writing options to %s", path.c_str());

    tree.put("rooms.api_version", "0");
    tree.put("permissions.allowX11Clients", allowX11Clients);
    tree.put("permissions.shareTempDir", shareTempDir);
    tree.put("permissions.shareHomeDir", shareHomeDir);
    tree.put("useLinuxABI", useLinuxABI);
    tree.put("isHidden", isHidden);
    tree.put("uuid", uuid);
    if (cloneUri != "") {
    	tree.put("cloneUri", cloneUri);
    }

    pt::write_json(path, tree);
}

void RoomOptions::dump()
{
	cout << "share_tmp=" << shareTempDir << endl;
	cout << "useLinuxABI=" << useLinuxABI << endl;
}

/* If the <src> options are different from the default,
 * merge them into the current object.
 *
 * FIXME: This does not account for removing permissions,
 * and assumes that permissions are only ever added.
 */
void RoomOptions::merge(const struct RoomOptions& src)
{
	if (src.allowX11Clients) {
		allowX11Clients = true;
		shareTempDir = true; // WORKAROUND: should generate a new xauth key instead
	}
	if (src.shareTempDir) {
		shareTempDir = true;
	}
	if (src.shareHomeDir) {
		shareHomeDir = true;
	}
}
