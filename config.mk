# slstatus version
VERSION = 1.1

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# flags
CPPFLAGS = -I$(X11INC) -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\" -DALSA
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -Os `pkg-config --cflags libnm libpulse libudev mm-glib upower-glib`
LDFLAGS  = -L$(X11LIB) -s -lasound `pkg-config --libs libnm libpulse libudev mm-glib upower-glib`
# OpenBSD: add -lsndio
# FreeBSD: add -lkvm -lsndio
LDLIBS   = -lX11

# compiler and linker
CC = cc
