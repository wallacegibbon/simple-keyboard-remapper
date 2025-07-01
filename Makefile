TARGET = simple-keyboard-remapper
INSTALL_PATH = /usr/bin
SYSTEM_PATH = /etc/systemd/system

OS_FILE = linux

ifeq ($(DEBUG), 1)
	C_FLAGS += -DDEBUG=1
endif

all: $(TARGET)

$(TARGET): remapper.o $(OS_FILE).o
	gcc -o $@ $^

remapper.o: remapper.c remapper.h keycode.h
	gcc -c -o $@ $< $(C_FLAGS)
$(OS_FILE).o: $(OS_FILE).c remapper.h
	gcc -c -o $@ $< $(C_FLAGS)

.PHONY: install uninstall clean showlog

install: $(TARGET)
	cp $< $(INSTALL_PATH)/
	cp $(TARGET).service $(SYSTEM_PATH)/
	./fix_device.sh $(SYSTEM_PATH)/$(TARGET).service
	systemctl daemon-reload
	systemctl enable $(TARGET).service
	systemctl start $(TARGET).service

uninstall:
	systemctl stop $(TARGET).service
	rm $(SYSTEM_PATH)/$(TARGET).service
	rm $(INSTALL_PATH)/$(TARGET)
	systemctl daemon-reload

clean:
	rm -f $(TARGET) *.o

showlog:
	journalctl -u $(TARGET) -f
