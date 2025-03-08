INSTALL_PATH = /usr/local/bin

all: simple-keyboard-remapper

simple-keyboard-remapper: remapper.c
	gcc -o $@ $< `pkg-config --cflags --libs libevdev`

.PHONY: install clean

install: simple-keyboard-remapper
	cp $< $(INSTALL_PATH)
	cp simple-keyboard-remapper.service /etc/systemd/system/
	systemctl enable simple-keyboard-remapper.service
	systemctl start simple-keyboard-remapper.service

uninstall:
	systemctl stop simple-keyboard-remapper.service
	rm /etc/systemd/system/simple-keyboard-remapper.service
	rm /usr/local/bin/simple-keyboard-remapper

clean:
	rm simple-keyboard-remapper
