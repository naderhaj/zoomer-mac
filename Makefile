# Use Apple's clang: it knows where the macOS SDK and frameworks live.
# -fblocks: ScreenCaptureKit and dispatch take blocks (^{ ... }) as callbacks.
CC      = /usr/bin/clang
CFLAGS  = -std=c11 -Wall -Wextra -O2 -fblocks
LDFLAGS = -lobjc \
          -framework AppKit -framework QuartzCore -framework ScreenCaptureKit \
          -framework CoreMedia -framework CoreVideo -framework IOSurface \
          -framework ApplicationServices -framework CoreGraphics \
          -framework CoreFoundation
PREFIX ?= /usr/local

SRCS = main.c tap.c capture.c overlay.c
HDRS = objc.h tap.h capture.h overlay.h

# Ad-hoc sign so macOS sees a stable code identity for permission prompts.
zoomer: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
	codesign -s - -f $@

run: zoomer
	./zoomer

install: zoomer
	install -m 755 zoomer $(PREFIX)/bin/zoomer

clean:
	rm -f zoomer a.out

.PHONY: run install clean
