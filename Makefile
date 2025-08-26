PROGRAM = simple-keyboard-remapper
BIN_PATH = /usr/bin
SYSTEM_PATH = /etc/systemd/system
CC = cc
STRIP = strip --remove-section=.eh_frame --remove-section=.eh_frame_hdr

#C_FLAGS = -DDEBUG=1
C_FLAGS =

OS_FILE = linux

all: $(PROGRAM)

$(PROGRAM): remapper.o $(OS_FILE).o
	@echo "	LINK	$@"
	@$(CC) -o $@ $^

remapper.o: remapper.c remapper.h keycode.h
	@echo "	CC	$@"
	@$(CC) -c -o $@ $< $(C_FLAGS)
$(OS_FILE).o: $(OS_FILE).c remapper.h
	@echo "	CC	$@"
	@$(CC) -c -o $@ $< $(C_FLAGS)

.PHONY: install uninstall clean showlog

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

clean:
	@rm -f $(PROGRAM) *.o

showlog:
	@journalctl -u $(PROGRAM) -f
