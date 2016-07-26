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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <locale>
#include <regex>
#include <string>
#include <streambuf>
#include <unordered_set>

extern "C" {
#include <getopt.h>
#include <jail.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
}

#include "namespaceImport.h"
#include "shell.h"
#include "fileUtil.h"
#include "room.h"
#include "roomManager.h"

FILE *logfile = NULL;
#include "logger.h"

void usage() {
	std::cout <<
		"Usage:\n\n"
		"  room <name> [create|destroy|enter]\n"
		" -or-\n"
		"  room <name> exec <arg0..argN>\n"
		" -or-\n"
		"  room [bootstrap|list]\n"
		"\n"
		"  Miscellaneous options:\n\n"
		"    -h, --help         This screen\n"
		"    -v, --verbose      Increase verbosity level\n"
	<< std::endl;
}

int
main(int argc, char *argv[])
{
	RoomConfig roomConfig;
	RoomManager mgr(roomConfig);
	char ch;
	static struct option longopts[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "verbose", no_argument, NULL, 'v' },
			{ NULL, 0, NULL, 0 }
	};

	logfile = fopen("/dev/null", "w");

	// Find an 'exec' argument and ignore everything after it
	// Without this, getopt_long() gets confused.
	int argc_before_exec = argc;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "exec")) {
			argc_before_exec = i;
			break;
		}
	}

	while ((ch = getopt_long(argc_before_exec, argv, "hv", longopts, NULL)) != -1) {
		switch (ch) {
		case 'h':
			usage();
			return 0;
			break;

		case 'v':
			fclose(logfile);
			logfile = stderr;
			break;

		default:
			usage();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	try {
		// Implicitly bootstrap OR (bootstrap explicitly AND exit)
		if (mgr.isBootstrapComplete()) {
			if (argc == 1 && string(argv[0]) == "bootstrap") {
				cout << "The bootstrap process is already complete. Nothing to do." << endl;
				exit(0);
			}
		} else {
			mgr.bootstrap();
			if (argc == 1 && string(argv[0]) == "bootstrap") {
				exit(0);
			}
		}

		mgr.setup();

		if (argc == 1) {
			std::string command = std::string(argv[0]);
			if (command == "list") {
				mgr.listRooms();
			} else if (command == "bootstrap") {
				throw std::logic_error("NOTREACHED");
			} else if (command == "--help" || command == "-h" || command == "help") {
				usage();
				exit(1);
			} else {
				throw std::runtime_error("Invalid command");
			}
		} else if (argc > 1) {
			std::string name = std::string(argv[0]);
			std::string command = std::string(argv[1]);

			if (command == "create") {
				mgr.cloneRoom(name);
			} else if (command == "destroy") {
				mgr.destroyRoom(name);
			} else if (command == "enter") {
				mgr.getRoomByName(name).enter();
			} else if (command == "exec") {
				mgr.getRoomByName(name).exec(argc, argv);
			} else {
				throw std::runtime_error("Invalid command");
			}
		}
	} catch(const std::system_error& e) {
		std::cout << "Caught system_error with code " << e.code()
	                  << " meaning " << e.what() << '\n';
		exit(1);
	} catch(const std::exception& e) {
		std::cout << "ERROR: Unhandled exception: " << e.what() << '\n';
		exit(1);
	} catch(...) {
		std::cout << "Unhandled exception\n";
		exit(1);
	}

	return EXIT_SUCCESS;
}
