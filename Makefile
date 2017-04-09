PKG_CONFIG	= pkg-config
WARNFLAGS	= -Wall -Werror
PNGFLAGS	!= ${PKG_CONFIG} --cflags libpng
REALCFLAGS	= ${CFLAGS} ${WARNFLAGS} ${PNGFLAGS} -Iinclude -g \
		-std=c99 -D_POSIX_C_SOURCE=200809L

LFLAGS		:= --nounistd

YACC		:= yacc
FLEX		:= flex
RM		:= rm -rf

# User-defined variables
PREFIX		= /usr/local
bindir		= ${PREFIX}/bin
mandir		= ${PREFIX}/man
Q		= @
STRIP		= -s
BINMODE		= 555
MANMODE		= 444

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
	src/link/lexer.o \
	src/link/library.o \
	src/link/main.o \
	src/link/mapfile.o \
	src/link/object.o \
	src/link/output.o \
	src/link/patch.o \
	src/link/parser.o \
	src/link/script.o \
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
	$Q${RM} rgbds.html gbz80.html
	$Q${RM} rgbasm rgbasm.exe ${rgbasm_obj} rgbasm.html
	$Q${RM} rgblink rgblink.exe ${rgblink_obj} rgblink.html rgblink-script.html
	$Q${RM} rgbfix rgbfix.exe ${rgbfix_obj} rgbfix.html
	$Q${RM} rgbgfx rgbgfx.exe ${rgbgfx_obj} rgbgfx.html
	$Q${RM} src/asm/asmy.c src/asm/asmy.h
	$Q${RM} src/link/lexer.c src/link/parser.c src/link/parser.h

install: all
	$Qmkdir -p ${DESTDIR}${bindir}
	$Qinstall ${STRIP} -m ${BINMODE} rgbasm ${DESTDIR}${bindir}/rgbasm
	$Qinstall ${STRIP} -m ${BINMODE} rgbfix ${DESTDIR}${bindir}/rgbfix
	$Qinstall ${STRIP} -m ${BINMODE} rgblink ${DESTDIR}${bindir}/rgblink
	$Qinstall ${STRIP} -m ${BINMODE} rgbgfx ${DESTDIR}${bindir}/rgbgfx
	$Qmkdir -p ${DESTDIR}${mandir}/man1 ${DESTDIR}${mandir}/man5 ${DESTDIR}${mandir}/man7
	$Qinstall -m ${MANMODE} src/rgbds.7 ${DESTDIR}${mandir}/man7/rgbds.7
	$Qinstall -m ${MANMODE} src/gbz80.7 ${DESTDIR}${mandir}/man7/gbz80.7
	$Qinstall -m ${MANMODE} src/asm/rgbasm.1 ${DESTDIR}${mandir}/man1/rgbasm.1
	$Qinstall -m ${MANMODE} src/fix/rgbfix.1 ${DESTDIR}${mandir}/man1/rgbfix.1
	$Qinstall -m ${MANMODE} src/link/rgblink.1 ${DESTDIR}${mandir}/man1/rgblink.1
	$Qinstall -m ${MANMODE} src/link/rgblink.5 ${DESTDIR}${mandir}/man5/rgblink.5
	$Qinstall -m ${MANMODE} src/gfx/rgbgfx.1 ${DESTDIR}${mandir}/man1/rgbgfx.1

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

.l.o:
	$Q${RM} $*.c
	$Q${FLEX} ${LFLAGS} -o $*.c $<
	$Q${CC} ${REALCFLAGS} -c -o $@ $*.c
	$Q${RM} $*.c

.c.o:
	$Q${CC} ${REALCFLAGS} -c -o $@ $<

src/asm/locallex.o src/asm/globlex.o src/asm/lexer.o: src/asm/asmy.h
src/asm/asmy.h: src/asm/asmy.c

src/link/lexer.o : src/link/parser.h
src/link/parser.h : src/link/parser.c

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
	$Qmandoc ${MANDOC} src/gbz80.7 | sed s/OpenBSD/General/ > gbz80.html
	$Qmandoc ${MANDOC} src/asm/rgbasm.1 | sed s/OpenBSD/General/ > \
		rgbasm.html
	$Qmandoc ${MANDOC} src/fix/rgbfix.1 | sed s/OpenBSD/General/ > \
		rgbfix.html
	$Qmandoc ${MANDOC} src/link/rgblink.1 | sed s/OpenBSD/General/ > \
		rgblink.html
	$Qmandoc ${MANDOC} src/link/rgblink.5 | sed s/OpenBSD/General/ > \
		rgblink-script.html
	$Qmandoc ${MANDOC} src/gfx/rgbgfx.1 | sed s/OpenBSD/General/ > \
		rgbgfx.html
