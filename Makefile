CFLAGS +=	-Wall -Iinclude -Iinclude/asm/gameboy -g -std=c99 \
			-D_POSIX_C_SOURCE=200112L
PREFIX ?=      /usr/local

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

rgblib_obj := \
	src/lib/library.o \
	src/lib/main.o

rgblink_obj := \
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

all: rgbasm rgblib rgblink rgbfix

clean:
	@rm -rf rgbasm $(rgbasm_obj)
	@rm -rf rgblib $(rgblib_obj)
	@rm -rf rgblink $(rgblink_obj)
	@rm -rf rgbfix $(rgbfix_obj)
	@rm -rf src/asm/asmy.c

install: all
	@install -s -o root -g bin -m 555 rgbasm ${PREFIX}/bin/rgbasm
	@install -s -o root -g bin -m 555 rgbfix ${PREFIX}/bin/rgbfix
	@install -s -o root -g bin -m 555 rgblink ${PREFIX}/bin/rgblink
	@install -s -o root -g bin -m 555 rgblib ${PREFIX}/bin/rgblib
	@install -o root -g bin -m 444 src/rgbds.7 ${PREFIX}/man/cat7/rgbds.7
	@install -o root -g bin -m 444 src/asm/rgbasm.1 \
		${PREFIX}/man/cat1/rgbasm.1
	@install -o root -g bin -m 444 src/fix/rgbfix.1 \
		${PREFIX}/man/cat1/rgbfix.1
	@install -o root -g bin -m 444 src/link/rgblink.1 \
		${PREFIX}/man/cat1/rgblink.1
	@install -o root -g bin -m 444 src/lib/rgblib.1 \
		${PREFIX}/man/cat1/rgblib.1

rgbasm: $(rgbasm_obj)
	@${CC} $(CFLAGS) -o $@ $(rgbasm_obj) -lm

rgblib: $(rgblib_obj)
	@${CC} $(CFLAGS) -o $@ $(rgblib_obj)

rgblink: $(rgblink_obj)
	@${CC} $(CFLAGS) -o $@ $(rgblink_obj)

rgbfix: $(rgbfix_obj)
	@${CC} $(CFLAGS) -o $@ $(rgbfix_obj)

.c.o:
	@${CC} $(CFLAGS) -c -o $@ $<

src/asm/asmy.c: src/asm/asmy.y
	@${YACC} -d -o $@ $<

src/asm/asmy.y: $(yacc_pre)
	@cat $(yacc_pre) > $@
