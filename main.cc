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

static void printUsage(po::options_description desc) {
	std::cout << "Usage:\n\n";
    std::cout << desc << std::endl;
}

static void get_options(int argc, char *argv[])
{
	RoomOptions roomOpt;
	RoomManager mgr;
	globalMgr = &mgr;

	string action;
	string roomName = "";
	string baseArchiveUri;
	bool isVerbose;

	string popt0, popt1, popt2, popt3;
	string runAsUser, upstreamUri;

	po::options_description desc("Miscellaneous options");
	desc.add_options()
	    ("help", "produce help message")
	    ("verbose,v", po::bool_switch(&isVerbose)->default_value(false), "increase verbosity")
	;

	po::options_description undocumented("Undocumented options");
	undocumented.add_options()
	    ("action", po::value<string>(&action), "the action to perform")
		("name", po::value<string>(&roomName), "the name of the room")
		("popt0", po::value<string>(&popt0), "positional opt 0")
		("popt1", po::value<string>(&popt1), "positional opt 1")
		("popt2", po::value<string>(&popt2), "positional opt 2")
		("popt3", po::value<string>(&popt3), "positional opt 3")
	;

	po::options_description exec_opts("Options when using exec");
	exec_opts.add_options()
	    ("user,u", po::value<string>(&runAsUser), "the user to run the command as")
	;

	po::options_description push_opts("Options when using push");
	push_opts.add_options()
	    ("set-upstream,u", po::value<string>(&upstreamUri), "the remote URI to push to ")
	;

	po::options_description create_opts("Options when creating");
	create_opts.add_options()
	    ("uri", po::value<string>(&baseArchiveUri), "the URI of the base.txz to install from")
	    ("clone", po::value<string>(&roomOpt.cloneUri), "the action to perform")
	    ("allow-x11", po::bool_switch(&roomOpt.allowX11Clients), "allow running X11 clients")
	    ("share-tempdir", po::bool_switch(&roomOpt.shareTempDir), "mount the global /tmp and /var/tmp inside the room")
		("share-home", po::bool_switch(&roomOpt.shareHomeDir), "mount the $HOME directory inside the room")
	;

/*
	po::options_description install_opts("Options when installing:");
	install_opts.add_options()
	    ("os", po::value<string>(&roomOpt.os_type), "the name-version-architecture of the OS")
	;
*/

	po::options_description all("All options");
	all.add(undocumented).add(create_opts).add(push_opts).add(desc);

	// Ignore all subsequent arguments after 'exec'
	int argc_before_exec = argc;
	bool found = false;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "exec")) {
			found = true;
			argc_before_exec = i + 1;
			break;
		}
	}
	if (found) {
		all.add(exec_opts);

		// If there is 'exec' and '--', use the '--' as a delimiter
		for (int i = 0; i < argc; i++) {
			if (!strcmp(argv[i], "--")) {
				argc_before_exec = i + 1;
				break;
			}
		}
	}

	po::positional_options_description p;
	p.add("popt0", 1);
	p.add("popt1", 1);
	p.add("popt2", 1);
	p.add("popt3", 1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc_before_exec, argv).
			options(all).positional(p).run(),
			vm);
	po::notify(vm);

	if (vm.count("help")) {
		po::options_description helpinfo;
		if (popt1 == "create") {
			helpinfo.add(create_opts);
		} else if (popt1 == "exec") {
			helpinfo.add(exec_opts);
		} else if (popt1 == "push") {
			helpinfo.add(push_opts);
		}
		helpinfo.add(desc);
		printUsage(helpinfo);
	    exit(0);
	}

	mgr.setVerbose(isVerbose);

	SetuidHelper::lowerPrivileges();

	mgr.parseConfig();

	// Special case: force bootstrapping as the first command
	if (! mgr.isBootstrapComplete()) {
		mgr.bootstrap();
	}

	mgr.initUserRoomSpace();

	if (popt0 == "list") {
		mgr.listRooms();
	} else if (popt0 == "clone") {
		string uri = popt1;
		roomName = popt2;
		mgr.cloneRoomFromRemote(roomName, uri);
	} else if (popt1 == "clone") {
		//LEGACY
		roomName = popt0;
		mgr.cloneRoomFromRemote(roomName, popt2);
	} else if (popt1 == "create") {
		roomName = popt0;
		if (roomOpt.cloneUri != "" && baseArchiveUri != "") {
			cout << "Error: cannot clone and install from a tarball at the same time\n";
			exit(1);
		}
		if (baseArchiveUri != "") {
			mgr.installRoom(roomName, baseArchiveUri, roomOpt);
		} else {
// DEADWOOD: need to redo the whole basetemplate mechanism
#if 0
			if (! mgr.doesBaseTemplateExist()) {
				cout << "ERROR: The FreeBSD-10.3 base template does not exist.\n";
				cout << "       See the room(1) manual page for an example of creating the base template.\n";
				exit(1);
				//mgr.createBaseTemplate();
			}
#endif
			mgr.cloneRoom(roomName, roomOpt);
		}
		Room room = mgr.getRoomByName(roomName);
	} else if (popt0 == "build") {
		SetuidHelper::dropPrivileges();
		execl("/usr/local/bin/ruby", "/usr/local/bin/ruby", "/usr/local/libexec/rooms/room-build", popt1.c_str(), NULL);
	} else if (popt1 == "configure") {
		Room room = mgr.getRoomByName(popt0);
		room.editConfiguration();
	} else if (popt0 == "init") {
		cout << "ERROR: the rooms subsystem has already been initialized\n";
		exit(1);
	} else if (popt1 == "destroy") {
		mgr.getRoomByName(popt0).destroy();
#if 0
		// not ready for prime time
	} else if (action == "import") {
		mgr.importRoom(roomName);
	} else if (action == "export") {
		mgr.getRoomByName(roomName).exportArchive();
#endif
	} else if (popt1 == "enter") {
		mgr.getRoomByName(popt0).enter();
	} else if (popt1 == "exec") {
		std::vector<std::string> execVec;
		for (int i = argc_before_exec; i < argc; i++) {
			execVec.push_back(argv[i]);
		}
		if (execVec.size() == 0) {
			cout << "ERROR: must specify a command to execute\n";
			exit(1);
		}
		mgr.getRoomByName(popt0).exec(execVec, runAsUser);
	} else if (popt1 == "push") {
		auto room = mgr.getRoomByName(popt0);
		if (upstreamUri != "") {
			room.setOriginUri(upstreamUri);
		}
		room.pushToOrigin();
	} else if (popt1 == "receive" || popt1 == "recv") {
		mgr.receiveRoom(popt0);
	} else if (popt1 == "snapshot") {
		if (popt2 == "list") {
			mgr.getRoomByName(popt0).printSnapshotList();
		} else {
			Room room = mgr.getRoomByName(popt0);

			string snapName = popt2;
			string action = popt3;
			if (action == "create") {
				room.snapshotCreate(snapName);
			} else if (action == "destroy") {
				room.snapshotDestroy(snapName);
			} else {
				cout << "ERROR: invalid syntax\n";
				exit(1);
			}
		}
	} else if (popt1 == "send") {
		mgr.getRoomByName(popt0).send();
	} else if (popt1 == "start") {
		mgr.getRoomByName(popt0).start();
	} else if (popt1 == "stop") {
		mgr.getRoomByName(popt0).stop();
	} else {
		cout << "ERROR: must specify a valid action\n";
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
