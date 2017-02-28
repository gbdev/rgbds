PKG_CONFIG =	pkg-config
WARNFLAGS =	-Wall -Werror=implicit
PNGFLAGS !=	${PKG_CONFIG} --cflags libpng
REALCFLAGS =	${CFLAGS} ${WARNFLAGS} ${PNGFLAGS} -Iinclude -g \
		-std=c99 -D_POSIX_C_SOURCE=200809L

# User-defined variables
PREFIX =	/usr/local
BINPREFIX =	${PREFIX}/bin
MANPREFIX =	${PREFIX}/man
Q =		@

rgbasm_obj = \
	src/asm/asmy.o \
	src/asm/charmap.o \
	src/asm/fstack.o \
	src/asm/globlex.o \
	src/asm/lexer.o \
	src/asm/main.o \
	src/asm/math.o \
	src/asm/output.o \
	src/asm/rpn.o \
	src/asm/symbol.o \
	src/asm/locallex.o \
	src/extern/err.o \
	src/extern/reallocarray.o \
	src/extern/strlcpy.o \
	src/extern/strlcat.o

rgblink_obj = \
	src/link/assign.o \
	src/link/library.o \
	src/link/main.o \
	src/link/mapfile.o \
	src/link/object.o \
	src/link/output.o \
	src/link/patch.o \
	src/link/symbol.o \
	src/extern/err.o

rgbfix_obj = \
	src/fix/main.o \
	src/extern/err.o

rgbgfx_obj = \
	src/gfx/gb.o \
	src/gfx/main.o \
	src/gfx/makepng.o \
	src/extern/err.o

all: rgbasm rgblink rgbfix rgbgfx

clean:
	$Qrm -rf rgbds.html
	$Qrm -rf rgbasm rgbasm.exe ${rgbasm_obj} rgbasm.html
	$Qrm -rf rgblink rgblink.exe ${rgblink_obj} rgblink.html
	$Qrm -rf rgbfix rgbfix.exe ${rgbfix_obj} rgbfix.html
	$Qrm -rf rgbgfx rgbgfx.exe ${rgbgfx_obj} rgbgfx.html
	$Qrm -rf src/asm/asmy.c src/asm/asmy.h

install: all
	$Qmkdir -p ${BINPREFIX}
	$Qinstall -s -m 555 rgbasm ${BINPREFIX}/rgbasm
	$Qinstall -s -m 555 rgbfix ${BINPREFIX}/rgbfix
	$Qinstall -s -m 555 rgblink ${BINPREFIX}/rgblink
	$Qinstall -s -m 555 rgbgfx ${BINPREFIX}/rgbgfx
	$Qmkdir -p ${MANPREFIX}/man1 ${MANPREFIX}/man7
	$Qinstall -m 444 src/rgbds.7 ${MANPREFIX}/man7/rgbds.7
	$Qinstall -m 444 src/asm/rgbasm.1 ${MANPREFIX}/man1/rgbasm.1
	$Qinstall -m 444 src/fix/rgbfix.1 ${MANPREFIX}/man1/rgbfix.1
	$Qinstall -m 444 src/link/rgblink.1 ${MANPREFIX}/man1/rgblink.1
	$Qinstall -m 444 src/gfx/rgbgfx.1 ${MANPREFIX}/man1/rgbgfx.1

rgbasm: ${rgbasm_obj}
	$Q${CC} ${REALCFLAGS} -o $@ ${rgbasm_obj} -lm

rgblink: ${rgblink_obj}
	$Q${CC} ${REALCFLAGS} -o $@ ${rgblink_obj}

rgbfix: ${rgbfix_obj}
	$Q${CC} ${REALCFLAGS} -o $@ ${rgbfix_obj}

rgbgfx: ${rgbgfx_obj}
	$Q${CC} ${REALCFLAGS} -o $@ ${rgbgfx_obj} `${PKG_CONFIG} --libs libpng`

.y.c:
	$Q${YACC} -d ${YFLAGS} -o $@ $<

.c.o:
	$Q${CC} ${REALCFLAGS} -c -o $@ $<

src/asm/locallex.o src/asm/globlex.o src/asm/lexer.o: src/asm/asmy.h
src/asm/asmy.h: src/asm/asmy.c

# Below is a target for the project maintainer to easily create win32 exes.
# This is not for Windows users!
# If you're building on Windows with Cygwin or Mingw, just follow the Unix
# install instructions instead.
mingw:
	$Qenv PATH=/usr/local/mingw32/bin:/bin:/usr/bin:/usr/local/bin \
		make WARNFLAGS= CC=gcc CFLAGS="-I/usr/local/mingw32/include \
			${CFLAGS}"
	$Qmv rgbasm rgbasm.exe
	$Qmv rgblink rgblink.exe
	$Qmv rgbfix rgbfix.exe
	$Qmv rgbgfx rgbgfx.exe

# Below is a target for the project maintainer to easily create web manuals.
# It relies on mandoc: http://mdocml.bsd.lv
MANDOC =	-Thtml -Ios=General -Oman=/rgbds/manual/%N/ \
			-Ostyle=/rgbds/manual/manual.css

wwwman:
	$Qmandoc ${MANDOC} src/rgbds.7 | sed s/OpenBSD/General/ > rgbds.html
	$Qmandoc ${MANDOC} src/asm/rgbasm.1 | sed s/OpenBSD/General/ > \
		rgbasm.html
	$Qmandoc ${MANDOC} src/fix/rgbfix.1 | sed s/OpenBSD/General/ > \
		rgbfix.html
	$Qmandoc ${MANDOC} src/link/rgblink.1 | sed s/OpenBSD/General/ > \
		rgblink.html
	$Qmandoc ${MANDOC} src/gfx/rgbgfx.1 | sed s/OpenBSD/General/ > \
		rgbgfx.html
