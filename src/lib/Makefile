obj := library.o main.o

all: xlib

clean:
	rm -rf xlib $(obj)

xlib: $(obj)
	gcc -Wall -o $@ $^

.c.o:
	gcc -Wall -I. -I.. -Iinclude -g -c -o $@ $<
