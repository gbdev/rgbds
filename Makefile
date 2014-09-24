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
	src/asm/asmy.o \
	src/asm/fstack.o \
	src/asm/globlex.o \
	src/asm/lexer.o \
	src/asm/main.o \
	src/asm/math.o \
	src/asm/output.o \
	src/asm/rpn.o \
	src/asm/symbol.o \
	src/asm/gameboy/locallex.o \
	src/extern/strlcpy.o \
	src/extern/strlcat.o

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
	$Qrm -rf rgbds.html
	$Qrm -rf rgbasm rgbasm.exe ${rgbasm_obj} rgbasm.html
	$Qrm -rf rgblib rgblib.exe ${rgblib_obj} rgblib.html
	$Qrm -rf rgblink rgblink.exe ${rgblink_obj} rgblink.html
	$Qrm -rf rgbfix rgbfix.exe ${rgbfix_obj} rgbfix.html
	$Qrm -rf src/asm/asmy.c src/asm/asmy.h src/asm/asmy.y

install: all
	$Qinstall -s -m 555 rgbasm ${BINPREFIX}/rgbasm
	$Qinstall -s -m 555 rgbfix ${BINPREFIX}/rgbfix
	$Qinstall -s -m 555 rgblink ${BINPREFIX}/rgblink
	$Qinstall -s -m 555 rgblib ${BINPREFIX}/rgblib
	$Qinstall -m 444 src/rgbds.7 ${MANPREFIX}/man7/rgbds.7 || \
		(echo Installing manpages to ${MANPREFIX} failed. >&2 && \
		 echo Check where your manpages are installed and set the \
		      proper directory >&2 && \
		 echo with, e.g., make install MANPREFIX=/usr/share/man \
		 >&2 ; false)
	$Qinstall -m 444 src/asm/rgbasm.1 \
		${MANPREFIX}/man1/rgbasm.1
	$Qinstall -m 444 src/fix/rgbfix.1 \
		${MANPREFIX}/man1/rgbfix.1
	$Qinstall -m 444 src/link/rgblink.1 \
		${MANPREFIX}/man1/rgblink.1
	$Qinstall -m 444 src/lib/rgblib.1 \
		${MANPREFIX}/man1/rgblib.1

rgbasm: ${rgbasm_obj}
	$Q${CC} ${CFLAGS} -o $@ ${rgbasm_obj} -lm

rgblib: ${rgblib_obj}
	$Q${CC} ${CFLAGS} -o $@ ${rgblib_obj}

rgblink: ${rgblink_obj}
	$Q${CC} ${CFLAGS} -o $@ ${rgblink_obj}

rgbfix: ${rgbfix_obj}
	$Q${CC} ${CFLAGS} -o $@ ${rgbfix_obj}

.c.o:
	$Q${CC} ${CFLAGS} -c -o $@ $<

src/asm/asmy.c: src/asm/asmy.y
	$Q${YACC} -d -o $@ $<

src/asm/asmy.y: ${yacc_pre}
	$Qcat ${yacc_pre} > $@


# Below is a target for the project maintainer to easily create win32 exes.
# This is not for Windows users!
# If you're building on Windows with Cygwin or Mingw, just follow the Unix
# install instructions instead.
mingw:
	$Qenv PATH=/usr/local/mingw32/bin:/bin:/usr/bin:/usr/local/bin make CC=gcc CFLAGS="-I/usr/local/mingw32/include ${CFLAGS}"
	$Qmv rgbasm rgbasm.exe
	$Qmv rgblib rgblib.exe
	$Qmv rgblink rgblink.exe
	$Qmv rgbfix rgbfix.exe

# Below is a target for the project maintainer to easily create web manuals.
# It relies on mandoc: http://mdocml.bsd.lv
MANDOC =	-Thtml -Oman=/rgbds/manual/%N/ -Ostyle=/rgbds/manual/manual.css -Ios=General

wwwman:
	$Qmandoc ${MANDOC} src/rgbds.7 | sed s/OpenBSD/General/ > rgbds.html
	$Qmandoc ${MANDOC} src/asm/rgbasm.1 | sed s/OpenBSD/General/ > rgbasm.html
	$Qmandoc ${MANDOC} src/fix/rgbfix.1 | sed s/OpenBSD/General/ > rgbfix.html
	$Qmandoc ${MANDOC} src/lib/rgblib.1 | sed s/OpenBSD/General/ > rgblib.html
	$Qmandoc ${MANDOC} src/link/rgblink.1 | sed s/OpenBSD/General/ > rgblink.html
