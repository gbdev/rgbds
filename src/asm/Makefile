all: rgbasm

asmy.y: yaccprt1.y gameboy/yaccprt2.y yaccprt3.y gameboy/yaccprt4.y
	cat $^ > $@

.y.c:
	bison -d -o $@ $^

.c.o:
	gcc -Wall -DGAMEBOY -I. -I.. -Iinclude -Igameboy -g -c -o $@ $<

rgbasm: $(patsubst %.c,%.o,$(wildcard *.c) $(wildcard gameboy/*.c))
	gcc -Wall -o $@ $^ -lm
