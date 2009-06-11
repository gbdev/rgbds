cflags = -Wall -Iinclude -Iinclude/asm/gameboy -g

all:

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
	src/rgbfix/main.o

all: rgbasm xlib xlink rgbfix

clean:
	rm -rf rgbasm $(rgbasm_obj)
	rm -rf xlib $(xlib_obj)
	rm -rf xlink $(xlink_obj)
	rm -rf rgbfix $(rgbfix_obj)

rgbasm: $(rgbasm_obj)
	gcc $(cflags) -o $@ $^ -lm

xlib: $(xlib_obj)
	gcc $(cflags) -o $@ $^

xlink: $(xlink_obj)
	gcc $(cflags) -o $@ $^

rgbfix: $(rgbfix_obj)
	gcc $(cflags) -o $@ $^

.c.o:
	gcc $(cflags) -DGAMEBOY -c -o $@ $<

.y.c:
	bison -d -o $@ $^

src/asm/asmy.y: src/asm/yaccprt1.y src/asm/gameboy/yaccprt2.y src/asm/yaccprt3.y src/asm/gameboy/yaccprt4.y
	cat $^ > $@
