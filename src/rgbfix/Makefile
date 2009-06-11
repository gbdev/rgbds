all: rgbfix

.c.o:
	gcc -Wall -I. -I.. -Iinclude -g -c -o $@ $<

rgbfix: $(patsubst %.c,%.o,$(wildcard *.c))
	gcc -Wall -o $@ $^

