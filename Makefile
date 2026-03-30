# Makefile for the Flying Air Fryers XScreenSaver hack
#
# Usage:
#   make              # build
#   ./airfryer        # test in a standalone window
#   sudo make install # install system-wide

CC      := gcc
BINARY  := airfryer

CFLAGS  := $(shell pkg-config --cflags x11 cairo) -Wall -Wextra -O2
LIBS    := $(shell pkg-config --libs   x11 cairo) -lm

PREFIX    ?= /usr
LIBDIR    := $(PREFIX)/libexec/xscreensaver
CONFIGDIR := $(PREFIX)/share/xscreensaver/config

# ── targets ─────────────────────────────────────────────────────────

.PHONY: all clean install uninstall

all: $(BINARY)

$(BINARY): airfryer.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(BINARY)

install: $(BINARY)
	install -d $(DESTDIR)$(LIBDIR)
	install -m 755 $(BINARY) $(DESTDIR)$(LIBDIR)/$(BINARY)
	install -d $(DESTDIR)$(CONFIGDIR)
	install -m 644 airfryer.xml $(DESTDIR)$(CONFIGDIR)/airfryer.xml
	@echo ""
	@echo "Installed.  Restart xscreensaver-demo to pick it up."

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(BINARY)
	rm -f $(DESTDIR)$(CONFIGDIR)/airfryer.xml
