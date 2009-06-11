obj := \
	assign.o \
	library.o \
	main.o \
	mapfile.o \
	object.o \
	output.o \
	patch.o \
	symbol.o

all: xlink

clean:
	rm -rf xlink $(obj)

xlink: $(obj)
	gcc -Wall -o $@ $^

.c.o:
	gcc -Wall -I. -I.. -Iinclude -g -c -o $@ $<

