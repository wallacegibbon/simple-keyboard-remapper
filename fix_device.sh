#! /bin/sh
#
# Resolve a *stable* device path for the systemd unit and write it into the
# unit file given as $1.
#
# The kernel assigns /dev/input/eventN dynamically: the number can change
# whenever the keyboard is (re-)enumerated (unplug/replug, USB reset/EMI, or
# a reboot).  A hard-coded eventN therefore breaks the service as soon as the
# number shifts.
#
# So we prefer the stable /dev/input/by-id/...-event-kbd symlink, whose name is
# derived from the device itself and stays the same across re-enumeration.  We
# only fall back to /dev/input/eventN when no by-id link is available.

set -e

unit=$1
if [ -z "$unit" ]; then
	echo "Usage: $0 <path-to-systemd-unit>" >&2
	exit 1
fi

# The first keyboard is the device whose handlers include "kbd".
event=$(sed -n 's/.*sysrq .*kbd .*\(event[0-9]\+\).*/\1/p' \
	/proc/bus/input/devices | head -n1)

if [ -z "$event" ]; then
	echo "Keyboard event file not found" >&2
	exit 1
fi

# Prefer the stable by-id symlink that points at this event device.
dev="/dev/input/$event"
event_real=$(readlink -f "/dev/input/$event" 2>/dev/null || echo "")
for link in /dev/input/by-id/*-event-kbd; do
	[ -e "$link" ] || continue
	if [ -n "$event_real" ] && [ "$(readlink -f "$link")" = "$event_real" ]; then
		dev=$link
		break
	fi
done

# Replace the device argument of the ExecStart line (idempotent).
sed -i "s|^\(ExecStart=.*simple-keyboard-remapper \).*|\1$dev|" "$unit"

echo "Keyboard device set to $dev"
