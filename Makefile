CC     = gcc
CFLAGS = -O2 -Wall
PKG    = gtk+-3.0
PREFIX = /usr/local

BIN    = xfce-store
APPID  = io.github.xfcestore.Store

all: $(BIN)

$(BIN): xfce-store.c
	$(CC) $(CFLAGS) -o $@ $< `pkg-config --cflags --libs $(PKG)`

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -Dm644 $(APPID).desktop $(DESTDIR)$(PREFIX)/share/applications/$(APPID).desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(PREFIX)/share/applications/$(APPID).desktop

clean:
	rm -f $(BIN)

.PHONY: all install uninstall clean
