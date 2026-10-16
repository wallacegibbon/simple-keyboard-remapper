# Simple Keyboard Remapper

On Linux, this program creates a new virtual keyboard device with
[uinput][uinput].  It grabs input from the physical keyboard and pipe events
to that virtual device.

The code is ready to be ported to other systems.


## Installation

If you only have one keyboard, the installation should be simple.

To build, install, enable and start the service:

```shell
sudo make install
```

If you have more than one keyboards, this program may not find the right
device, then you need to change the systemd file to fix that.

### Choosing the right device (stable path)

The install step runs `fix_device.sh`, which fills the actual device into the
unit's `ExecStart`.  The kernel assigns `/dev/input/eventN` numbers dynamically,
so they can change on every (re-)enumeration — unplug/replug, a USB reset, or
even a reboot.  A hard-coded `eventN` therefore breaks the service whenever the
number shifts (typically with `Failed to open the source keyboard: No such file
or directory`, followed by a restart loop).

To avoid that, the script prefers the stable by-id symlink:

```
/dev/input/by-id/usb-<Vendor>_<Product>-event-kbd
```

Its name comes from the device itself and survives re-enumeration, so the
service keeps working.  If no by-id link exists it falls back to
`/dev/input/eventN`.

To list the candidates by hand:

```sh
ls -l /dev/input/by-id/*-event-kbd
cat /proc/bus/input/devices | grep -i keyboard -B1 -A4
```

The unit also sets `Restart=always` + `RestartSec=1` + `StartLimitIntervalSec=0`
so that if the keyboard is temporarily gone the service retries and re-grabs it
once it is back, instead of giving up after the systemd start-rate limit.

To stop, remove the service and uninstall the program:

```shell
sudo make uninstall
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

This project was modified from [Janus Key][janus-key].

The `libevdev` dependency is removed, we use `uinput` directly.

[uinput]: https://www.kernel.org/doc/html/v4.12/input/uinput.html
[janus-key]: https://github.com/pietroiusti/janus-key
