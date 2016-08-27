# Disclaimer

**_WARNING_** This is a developer preview and should not be used for
anything important. It may destroy your data or worse. Use at your own risk!

Pay close attention to this part of the license:

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


# Overview

Features:
1. Allows unprivileged users to create jails in a simple and safe manner
2. Allows running X11 applications in jails

# Requirements

To build on FreeBSD, you will need:
 * FreeBSD 10.3 or newer
 * the boost-libs package

# Installation

```
make
sudo make install
room init
```

# Usage

TODO fill out this section.

# Examples

Initialize the rooms subsystem. This only needs to be done once, the first
time you use the room(1) command.
	```
	$ room init
	```

Create a new room called 'hello_world'.
	```
	$ room hello_world create
	```

Enter the room. This will launch a shell inside the room.
	```
	$ room hello_world create
	```

Install the 'xclock' program.
	```
	$ su -
	# pkg install -y xclock
	# exit
	``

Run the 'xclock' program. It should display a small clock on your desktop.
	```
	$ xclock
	```

When you are done, exit the shell, and destroy the room.
	```
	$ exit
	$ room hello_world destroy
	```
