all: xlink

.c.o:
	gcc -Wall -I. -I.. -Iinclude -g -c -o $@ $<

xlink: $(patsubst %.c,%.o,$(wildcard *.c))
	gcc -Wall -o $@ $^

