all:
	make -C src/asm rgbasm
	make -C src/lib xlib
	make -C src/link xlink
	make -C src/rgbfix rgbfix

clean:
	make -C src/asm clean
	make -C src/lib clean
	make -C src/link clean
	make -C src/rgbfix clean
