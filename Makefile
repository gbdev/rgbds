localversion = $(shell sh scripts/localversion.sh)

cflags = -Wall -Iinclude -Iinclude/asm/gameboy -DLOCALVERSION=\"$(localversion)\" -g -std=c99 -D_POSIX_C_SOURCE=200112L

all:

yacc_pre := \
	src/asm/yaccprt1.y\
	src/asm/gameboy/yaccprt2.y\
	src/asm/yaccprt3.y\
	src/asm/gameboy/yaccprt4.y

rgbasm_obj := \
	src/asm/alloca.o \
	src/asm/asmy.o \
	src/asm/fstack.o \
	src/asm/globlex.o \
	src/asm/lexer.o \
	src/asm/main.o \
	src/asm/math.o \
	src/asm/output.o \
	src/asm/rpn.o \
	src/asm/symbol.o \
	src/asm/gameboy/locallex.o

xlib_obj := \
	src/lib/library.o \
	src/lib/main.o

xlink_obj := \
	src/link/assign.o \
	src/link/library.o \
	src/link/main.o \
	src/link/mapfile.o \
	src/link/object.o \
	src/link/output.o \
	src/link/patch.o \
	src/link/symbol.o

rgbfix_obj := \
	src/fix/main.o

all: rgbasm xlib xlink rgbfix

clean:
	rm -rf rgbasm $(rgbasm_obj)
	rm -rf xlib $(xlib_obj)
	rm -rf xlink $(xlink_obj)
	rm -rf rgbfix $(rgbfix_obj)
	rm -rf src/asm/asmy.c

rgbasm: $(rgbasm_obj)
	${CC} $(cflags) -o $@ $(rgbasm_obj) -lm

xlib: $(xlib_obj)
	${CC} $(cflags) -o $@ $(xlib_obj)

xlink: $(xlink_obj)
	${CC} $(cflags) -o $@ $(xlink_obj)

rgbfix: $(rgbfix_obj)
	${CC} $(cflags) -o $@ $(rgbfix_obj)

.c.o:
	${CC} $(cflags) -DGAMEBOY -c -o $@ $<

src/asm/asmy.c: src/asm/asmy.y
	${YACC} -d -o $@ $<

src/asm/asmy.y: $(yacc_pre)
	cat $(yacc_pre) > $@
