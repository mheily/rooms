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

#include <boost/program_options.hpp>

extern "C" {
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
#include "setuidHelper.h"

FILE *logfile = NULL;
#include "logger.h"

RoomManager* globalMgr;

using std::pair;

namespace po = boost::program_options;

static const std::vector<string> actions = {
		"clone", "create", "destroy", "enter", "exec",
		"init", "list",
		"export", "import",
};

// KLUDGE: horrible temporary storage for CLI options
static struct {
	bool allow_x11;
	bool share_tempdir;
	bool share_home;
	/*string os_type;*/
} roomOpt;

static bool isValidAction(const string& s)
{
    return (std::any_of(actions.begin(), actions.end(),
    		[&s](string action){ return s == action; }));
}

static pair<string, string> parseAction(const string& s)
{
    if (isValidAction(s)) {
    	return make_pair(string("action"), s);
    } else if (globalMgr->checkRoomExists(s)) { // XXX-should validate name first
        return make_pair(string("name"), s);
    } else {
        return make_pair(string(), string());
    }
}

static void printUsage(po::options_description desc) {
	std::cout <<
		"Usage:\n\n"
		"  room <name> [create|destroy|enter|export]\n"
		" -or-\n"
		"  room <name> exec <command> [arguments]\n"
		" -or-\n"
		"  room import --name=<name>\n"
		" -or-\n"
		"  room [init|list]\n"
		"\n"
	<< std::endl;

    std::cout << desc << std::endl;
}

static void apply_room_options(po::variables_map vm, Room room)
{
	RoomOptions ro = room.getRoomOptions();

	if (vm.count("allow-x11")) {
		ro.allowX11Clients = roomOpt.allow_x11;
	}
	if (vm.count("share-tempdir")) {
		ro.shareTempDir = roomOpt.share_tempdir;
	}
	if (vm.count("share-home")) {
		ro.shareHomeDir = roomOpt.share_home;
	}
	room.syncRoomOptions();
}

static void get_options(int argc, char *argv[])
{
	RoomManager mgr;
	globalMgr = &mgr;

	string action;
	string roomName = "";
	bool isVerbose;

	po::options_description desc("Allowed options");
	desc.add_options()
	    ("help", "produce help message")
	    ("allow-x11", po::bool_switch(&roomOpt.allow_x11), "allow running X11 clients")
	    ("share-tempdir", po::bool_switch(&roomOpt.share_tempdir), "mount the global /tmp and /var/tmp inside the room")
		("share-home", po::bool_switch(&roomOpt.share_home), "mount the $HOME directory inside the room")
	    ("verbose,v", po::bool_switch(&isVerbose)->default_value(false), "increase verbosity")
	;

	po::options_description undocumented("Undocumented options");
	undocumented.add_options()
	    ("action", po::value<string>(&action), "the action to perform")
		("name", po::value<string>(&roomName), "the name of the room")
	;

	po::options_description clone_opts("Options when cloning:");
	clone_opts.add_options()
	    ("source", po::value<string>(&action), "the action to perform")
	;

/*
	po::options_description install_opts("Options when installing:");
	install_opts.add_options()
	    ("os", po::value<string>(&roomOpt.os_type), "the name-version-architecture of the OS")
	;
*/

	po::options_description all("Allowed options");
	all.add(desc).add(undocumented).add(clone_opts);

	// If this is an 'exec' command, ignore all subsequent arguments
	// TODO: should add support for '--' so you could say:
	//     room foo exec -u some_user -- /bin/sh
	int argc_before_exec = argc;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "exec")) {
			argc_before_exec = i + 1;
			break;
		}
	}

	po::variables_map vm;
	po::store(po::command_line_parser(argc_before_exec, argv).
			options(all).extra_parser(parseAction).run(),
			vm);
	po::notify(vm);

	if (vm.count("help")) {
		printUsage(desc);
	    exit(0);
	}

	mgr.setVerbose(isVerbose);

	SetuidHelper::lowerPrivileges();

	// Special case: force bootstrapping as the first command
	if (! mgr.isBootstrapComplete()) {
		if (action == "init") {
			mgr.bootstrap();
			exit(0);
		} else {
			cout << "ERROR: please run 'room init' to initialize the rooms subsystem\n";
			exit(1);
		}
	}

	// Get the room and allow overriding the room options via the CLI
	if (roomName != "" && mgr.checkRoomExists(roomName)) {
		Room room = mgr.getRoomByName(roomName);
		apply_room_options(vm, room);
	}

	// Most actions require the name of an existing room
	if (roomName == "" && (action != "create" && action != "list" && action != "init")) {
		cout << "ERROR: must specify the name of the room\n";
		printUsage(desc);
		exit(1);
	}

	if (action == "list") {
		mgr.listRooms();
	} else if (action == "create") {
		if (! mgr.doesBaseTemplateExist()) {
			mgr.createBaseTemplate();
		}
		mgr.cloneRoom(roomName);
		Room room = mgr.getRoomByName(roomName);
		apply_room_options(vm, room);
	} else if (action == "init") {
		cout << "ERROR: the rooms subsystem has already been initialized\n";
		exit(1);
	} else if (action == "destroy") {
		mgr.getRoomByName(roomName).destroy();
	} else if (action == "import") {
		mgr.importRoom(roomName);
	} else if (action == "export") {
		mgr.getRoomByName(roomName).exportArchive();
	} else if (action == "enter") {
		mgr.getRoomByName(roomName).enter();
	} else if (action == "exec") {
		std::vector<std::string> execVec;
		for (int i = argc_before_exec; i < argc; i++) {
			execVec.push_back(argv[i]);
		}
		if (execVec.size() == 0) {
			cout << "ERROR: must specify a command to execute\n";
			exit(1);
		}
		mgr.getRoomByName(roomName).exec(execVec);
	} else {
		cout << "ERROR: must specify an action\n";
		printUsage(desc);
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	try {
		SetuidHelper::checkPrivileges();
		logfile = fopen("/dev/null", "w");
		get_options(argc, argv);
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
