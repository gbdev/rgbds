all: xlib

.c.o:
	gcc -Wall -I. -I.. -Iinclude -g -c -o $@ $<

xlib: $(patsubst %.c,%.o,$(wildcard *.c))
	gcc -Wall -o $@ $^
