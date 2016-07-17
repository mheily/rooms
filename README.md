

# TODO

Patch pkg(8) to not crap out if things aren't owned by root.
Examples:

$ pkg install a
The package management tool is not yet installed on your system.
Do you want to fetch and install it now? [y/N]: y
Bootstrapping pkg from pkg+http://pkg.FreeBSD.org/FreeBSD:11:amd64/latest, please wait...
Verifying signature with trusted certificate pkg.freebsd.org.2013102301... done
pkg: failed to extract pkg-static: Can't set UID=0

# pkg install fakeroot
The package management tool is not yet installed on your system.
Do you want to fetch and install it now? [y/N]: y
Bootstrapping pkg from pkg+http://pkg.FreeBSD.org/FreeBSD:11:amd64/latest, please wait...
Verifying signature with trusted certificate pkg.freebsd.org.2013102301... done
pkg-static: /var/db/pkg wrong user or group ownership (expected 0/0 versus actual 101001/0)

