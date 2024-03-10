#include <stdio.h>

/**
 * MinGW mangles path names before passing them as command-line arguments.
 * Some RGBLINK warning/error messages include those mangled paths on Windows.
 * We need to see those mangled paths in test.sh to replace them with placeholders.
 * This tool simply echoes each argument, which will be mangled iff they are paths.
 * (For example, the "/tmp/foo" will be unmangled to something like
 * "C:/Users/RUNNER~1/AppData/Local/Temp/foo".)
 */

int main(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++)
		puts(argv[i]);
	return 0;
}
