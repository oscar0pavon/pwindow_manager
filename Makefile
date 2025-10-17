# pwindow_manager a modifed dwm source code
# See LICENSE file for copyright and license details.


# Customize below to fit your system

CC = cc

# paths
PREFIX = /usr/

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

# includes and libs
INCS = -I${X11INC} -I${FREETYPEINC}
LIBS = -L${X11LIB} -lX11 -lXinerama ${FREETYPELIBS}

# flags
DEFINES = -D_XOPEN_SOURCE=700L
#CFLAGS   = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS   = -std=c11 -Wall -Wno-deprecated-declarations -Os ${INCS} ${DEFINES}
LDFLAGS  = ${LIBS}


SRC = draw.c main.c util.c monitors.c windows.c input.c events.c bar.c
OBJ = ${SRC:.c=.o}

all: pwindow_manager

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h


pwindow_manager: ${OBJ}
	${CC} -o $@ ${OBJ} ${CFLAGS} ${LDFLAGS}

clean:
	rm -f pwindow_manager
	rm -f *.o


install: all
	pkill pwindow_manager
	cp pwindow_manager /usr/bin
	pwindow_manager &

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/pwindow_manager

.PHONY: all clean dist install uninstall
