#! /bin/sh

first_kbd=$(cat /proc/bus/input/devices \
	| sed -n 's/.*sysrq .*kbd .*\(event[0-9]\+\).*/\1/p' \
	| head -n1)

case $first_kbd in
event*)
	;;
*)
	echo "Keyboard event file not found" >&2
	exit 1
	;;
esac

svc_file=/etc/systemd/system/simple-keyboard-remapper.service

echo "Replacing event0 with $first_kbd ..."
sed -i "s/event0/$first_kbd/" $svc_file
