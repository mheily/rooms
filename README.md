# Installation

```
make
sudo make install
```

# Usage

# Examples

Create a new room called 'hello_world'.
```
$ room create hello_world
```

Enter the room. This will launch a shell inside the room.
```
$ room enter hello_world
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
$ room destroy hello_world
```
