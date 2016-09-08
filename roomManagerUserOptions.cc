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

#include "roomManagerUserOptions.h"
#include "logger.h"

namespace pt = boost::property_tree;

void RoomManagerUserOptions::load(const string &path) {
	pt::ptree tree;

	log_debug("reading options from %s", path.c_str());
	pt::read_json(path, tree);

	allowX11Clients = tree.get("permissions.default.allow_x11", false);
	shareTempDir = tree.get("permissions.default.share_tempdir", false);
	shareHomeDir = tree.get("permissions.default.share_home", false);
}

void RoomManagerUserOptions::save(const string &path) {
	pt::ptree tree;

	log_debug("writing options to %s", path.c_str());

	tree.put("permissions.default.allow_x11", allowX11Clients);
	tree.put("permissions.default.share_tempdir", shareTempDir);
	tree.put("permissions.default.share_home", shareHomeDir);

	pt::write_json(path, tree);
}
