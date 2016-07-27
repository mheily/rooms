# Overview

Features:
1. Allows unprivileged users to create jails in a simple and safe manner
2. Allows running X11 applications in jails

# Installation

```
make
sudo make install
```

# Usage

# Examples

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
