CFLAGS +=	-Wall -Iinclude -Iinclude/asm/gameboy -g -std=c99

# User-defined variables
PREFIX =	/usr/local
BINPREFIX =	${PREFIX}/bin
MANPREFIX =	${PREFIX}/man
Q =		@

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
	${Q}rm -rf rgbasm $(rgbasm_obj)
	${Q}rm -rf rgblib $(rgblib_obj)
	${Q}rm -rf rgblink $(rgblink_obj)
	${Q}rm -rf rgbfix $(rgbfix_obj)
	${Q}rm -rf src/asm/asmy.c

install: all
	${Q}install -s -m 555 rgbasm ${BINPREFIX}/rgbasm
	${Q}install -s -m 555 rgbfix ${BINPREFIX}/rgbfix
	${Q}install -s -m 555 rgblink ${BINPREFIX}/rgblink
	${Q}install -s -m 555 rgblib ${BINPREFIX}/rgblib
	${Q}install -m 444 src/rgbds.7 ${MANPREFIX}/man7/rgbds.7
	${Q}install -m 444 src/asm/rgbasm.1 \
		${MANPREFIX}/man1/rgbasm.1
	${Q}install -m 444 src/fix/rgbfix.1 \
		${MANPREFIX}/man1/rgbfix.1
	${Q}install -m 444 src/link/rgblink.1 \
		${MANPREFIX}/man1/rgblink.1
	${Q}install -m 444 src/lib/rgblib.1 \
		${MANPREFIX}/man1/rgblib.1

rgbasm: $(rgbasm_obj)
	${Q}${CC} $(CFLAGS) -o $@ $(rgbasm_obj) -lm

rgblib: $(rgblib_obj)
	${Q}${CC} $(CFLAGS) -o $@ $(rgblib_obj)

rgblink: $(rgblink_obj)
	${Q}${CC} $(CFLAGS) -o $@ $(rgblink_obj)

rgbfix: $(rgbfix_obj)
	${Q}${CC} $(CFLAGS) -o $@ $(rgbfix_obj)

.c.o:
	${Q}${CC} $(CFLAGS) -c -o $@ $<

src/asm/asmy.c: src/asm/asmy.y
	${Q}${YACC} -d -o $@ $<

src/asm/asmy.y: $(yacc_pre)
	${Q}cat $(yacc_pre) > $@
