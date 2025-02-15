# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c pwindow_manager.c util.c
OBJ = ${SRC:.c=.o}

all: pwindow_manager

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk


pwindow_manager: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f pwindow_manager ${OBJ} dwm-${VERSION}.tar.gz


install: all
	pkill pwindow_manager
	cp pwindow_manager /usr/local/bin
	pwindow_manager &

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/pwindow_manager\
		${DESTDIR}${MANPREFIX}/man1/dwm.1

.PHONY: all clean dist install uninstall
