PROGRAM = simple-keyboard-remapper
BIN_PATH = /usr/bin
SYSTEM_PATH = /etc/systemd/system
CC = cc
STRIP = strip --remove-section=.eh_frame --remove-section=.eh_frame_hdr

#CFLAGS = -O2 -DDEBUG=1
CFLAGS = -O2

OS_FILE = linux

all: $(PROGRAM)

$(PROGRAM): remapper.o $(OS_FILE).o
	@echo "	LINK	$@"
	@$(CC) -o $@ $^

clean:
	@rm -f $(PROGRAM) *.o

install: $(PROGRAM)
	@cp $(PROGRAM) $(BIN_PATH)
	@$(STRIP) $(BIN_PATH)/$(PROGRAM)
	@chmod 755 $(BIN_PATH)/$(PROGRAM)
	@cp $(PROGRAM).service $(SYSTEM_PATH)/
	@./fix_device.sh $(SYSTEM_PATH)/$(PROGRAM).service
	@systemctl daemon-reload
	@systemctl enable $(PROGRAM).service
	@systemctl start $(PROGRAM).service

uninstall:
	@systemctl stop $(PROGRAM).service
	@rm $(SYSTEM_PATH)/$(PROGRAM).service
	@rm $(BIN_PATH)/$(PROGRAM)
	@systemctl daemon-reload

showlog:
	@journalctl -u $(PROGRAM) -f

.c.o:
	@echo "	CC	$@"
	@$(CC) $(CFLAGS) -c $*.c

remapper.o: remapper.c remapper.h keycode.h
linux.o: linux.c remapper.h
