CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror
PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

PROTO_XML := ../default/dwl/protocols/dwl-ipc-unstable-v2.xml
PROTO_H := dwl-ipc-unstable-v2-protocol.h
PROTO_C := dwl-ipc-unstable-v2-protocol.c

XDG_OUT_XML := ../default/dwl/protocols/xdg-output-unstable-v1.xml
XDG_OUT_H := xdg-output-unstable-v1-protocol.h
XDG_OUT_C := xdg-output-unstable-v1-protocol.c

WL_CFLAGS := $(shell $(PKG_CONFIG) --cflags wayland-client)
WL_LIBS := $(shell $(PKG_CONFIG) --libs wayland-client)

TARGET := ../libexec/quattro-dwl-ipc

.PHONY: all clean

all: $(TARGET)

$(PROTO_H): $(PROTO_XML)
	$(WAYLAND_SCANNER) client-header $< $@

$(PROTO_C): $(PROTO_XML)
	$(WAYLAND_SCANNER) private-code $< $@

$(XDG_OUT_H): $(XDG_OUT_XML)
	$(WAYLAND_SCANNER) client-header $< $@

$(XDG_OUT_C): $(XDG_OUT_XML)
	$(WAYLAND_SCANNER) private-code $< $@

$(TARGET): quattro-dwl-ipc.c $(PROTO_C) $(PROTO_H) $(XDG_OUT_C) $(XDG_OUT_H)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(WL_CFLAGS) -I. -o $@ quattro-dwl-ipc.c $(PROTO_C) $(XDG_OUT_C) $(WL_LIBS)

clean:
	rm -f $(TARGET) $(PROTO_H) $(PROTO_C) $(XDG_OUT_H) $(XDG_OUT_C)
