# Simple Keyboard Remapper

On Linux, this program creates a new virtual keyboard device with `uinput`.
It grabs input from the physical keyboard and pipe events to that virtual
device.

The code is ready to be ported to other systems.


## Installation

If you only have one keyboard, the installation should be simple.

To build, install, enable and start the service:

```shell
make install
```

If you have more than one keyboards, this program may not find the right
device, then you need to change the systemd file to fix that.

To stop, remove the service and uninstall the program:

```shell
make uninstall
```


## Debug

We can see event printing when this project is built with DEBUG enabled:
```shell
make DEBUG=1
```

After installation, we can read the log with this command:

```shell
journalctl -u simple-keyboard-remapper -f
```

Or: (In the project path)

```shell
make showlog
```

To find out the event file that our keyboard is bound to:
```sh
cat /proc/bus/input/devices | grep -i keyboard -B1 -A4
```


## History

This project was modified from [Janus Key](https://github.com/pietroiusti/janus-key).

The `libevdev` dependency is removed, we use `uinput` directly.
