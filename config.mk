# menu-typo build settings

VERSION = 0.1.0
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib

# dmenu the patch is made for
DMENU_VERSION = 5.4
DMENU_SHA256 = 8fbace2a0847aa80fe861066b118252dcc7b4ca0a0a8f3a93af02da8fb6cd453
DMENU_URL = https://dl.suckless.org/tools/dmenu-$(DMENU_VERSION).tar.gz

CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -pedantic
ROFI_CFLAGS = $(shell $(PKG_CONFIG) --cflags rofi glib-2.0 gio-unix-2.0 json-glib-1.0 cairo) -fPIC
ROFI_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0 gio-2.0 json-glib-1.0)
ROFI_PLUGINDIR = $(shell $(PKG_CONFIG) --variable=pluginsdir rofi)
