obj := \
	main.o

all: rgbfix

clean:
	rm -rf rgbfix $(obj)

rgbfix: $(obj)
	gcc -Wall -o $@ $^

.c.o:
	gcc -Wall -I../../include -g -c -o $@ $<
