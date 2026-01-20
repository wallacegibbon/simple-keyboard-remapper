#! /bin/sh

first_kbd=$(cat /proc/bus/input/devices \
	| sed -n 's/.*sysrq .*kbd .*\(event[0-9]\+\).*/\1/p' \
	| head -n1)

case $first_kbd in
event0)
	echo "First keyboard is event0, no need to replace."
	exit 0
	;;
event*)
	echo "Replacing event0 with $first_kbd ..."
	sed -i "s/event0/$first_kbd/" $1
	;;
*)
	echo "Keyboard event file not found" >&2
	exit 1
	;;
esac
