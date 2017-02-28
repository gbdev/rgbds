# GNU Make 3.x doesn't support the "!=" shell syntax, so here's an alternative

PKG_CONFIG = pkg-config
PNGFLAGS = $(shell ${PKG_CONFIG} --cflags libpng)

include Makefile
