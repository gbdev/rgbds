obj := \
	alloca.o \
	asmy.o \
	fstack.o \
	globlex.o \
	lexer.o \
	main.o \
	math.o \
	output.o \
	rpn.o \
	symbol.o \
	gameboy/locallex.o

all: rgbasm

clean:
	rm -rf rgbasm $(obj) asmy.y asmy.c asmy.h

rgbasm: $(obj)
	gcc -Wall -o $@ $^ -lm

.c.o:
	gcc -Wall -DGAMEBOY -I. -I.. -Iinclude -Igameboy -g -c -o $@ $<

.y.c:
	bison -d -o $@ $^

asmy.y: yaccprt1.y gameboy/yaccprt2.y yaccprt3.y gameboy/yaccprt4.y
	cat $^ > $@
