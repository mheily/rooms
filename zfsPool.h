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

#include <unistd.h>

#include "namespaceImport.h"
#include "fileUtil.h"
#include "shell.h"

class ZfsPool {
public:
	static bool detectZfs();
	static bool validateName(const string& name, string& errorMsg) {
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

	static string getNameByPath(const string& path);
};
