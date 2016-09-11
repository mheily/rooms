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

	allowX11Clients = tree.get("allowX11Clients", false);
	shareTempDir = tree.get("shareTempDir", false);
	shareHomeDir = tree.get("shareHomeDir", false);
	useLinuxABI = tree.get("useLinuxABI", false);
	isHidden = tree.get("isHidden", false);
}


void RoomOptions::save(const string &path)
{
    pt::ptree tree;

    log_debug("writing options to %s", path.c_str());

    tree.put("allowX11Clients", allowX11Clients);
    tree.put("shareTempDir", shareTempDir);
    tree.put("shareHomeDir", shareHomeDir);
    tree.put("useLinuxABI", useLinuxABI);
    tree.put("isHidden", isHidden);

    pt::write_json(path, tree);
}

void RoomOptions::dump()
{
	cout << "share_tmp=" << shareTempDir << endl;
	cout << "useLinuxABI=" << useLinuxABI << endl;
}
