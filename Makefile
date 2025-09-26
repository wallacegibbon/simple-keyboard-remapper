PROGRAM = simple-keyboard-remapper
BIN = /usr/bin
SYSTEM = /etc/systemd/system
CC = cc
STRIP = strip --remove-section=.eh_frame --remove-section=.eh_frame_hdr

#CFLAGS = -O2 -DGRAVE_IS_ESC -DEMACS_USER #-DDEBUG=1
CFLAGS = -O2 -DGRAVE_IS_ESC
#CFLAGS = -O2

OBJS = remapper.o linux.o

${PROGRAM}: ${OBJS}
	${CC} -o $@ ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f ${PROGRAM} *.o

install: ${PROGRAM}
	cp ${PROGRAM} ${BIN}
	${STRIP} ${BIN}/${PROGRAM}
	chmod 755 ${BIN}/${PROGRAM}
	cp ${PROGRAM}.service ${SYSTEM}/
	./fix_device.sh ${SYSTEM}/${PROGRAM}.service
	systemctl daemon-reload
	systemctl enable ${PROGRAM}.service
	systemctl start ${PROGRAM}.service

uninstall:
	systemctl stop ${PROGRAM}.service
	rm ${SYSTEM}/${PROGRAM}.service
	rm ${BIN}/${PROGRAM}
	systemctl daemon-reload

showlog:
	journalctl -u ${PROGRAM} -f

remapper.o: remapper.c remapper.h keycode.h
linux.o: linux.c remapper.h
