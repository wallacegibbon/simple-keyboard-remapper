INSTALL_PATH = /usr/local/bin
C_FLAGS := $(shell pkg-config --cflags libevdev)
LD_FLAGS := $(shell pkg-config --libs libevdev)

all: simple-keyboard-remapper

simple-keyboard-remapper: remapper.o time_util.o
	gcc -o $@ $^ $(LD_FLAGS)

remapper.o: remapper.c time_util.h
	gcc -c -o $@ $< $(C_FLAGS)
time_util.o: time_util.c time_util.h
	gcc -c -o $@ $< $(C_FLAGS)

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
	rm -f simple-keyboard-remapper *.o
