#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "asmotor.h"

#include "lib/types.h"
#include "lib/library.h"

/*
 * Print the usagescreen
 *
 */

void 
PrintUsage(void)
{
	printf("RGBLib v" LIB_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\n"
	    "Usage: rgblib library command [module1 module2 ... modulen]\n"
	    "Commands:\n\ta\tAdd/replace modules to library\n"
	    "\td\tDelete modules from library\n"
	    "\tl\tList library contents\n"
	    "\tx\tExtract modules from library\n");
	exit(EX_USAGE);
}
/*
 * The main routine
 *
 */

int 
main(int argc, char *argv[])
{
	SLONG argn = 0;
	char *libname;

	argc -= 1;
	argn += 1;

	if (argc >= 2) {
		sLibrary *lib;

		lib = lib_Read(libname = argv[argn++]);
		argc -= 1;

		if (strcmp(argv[argn], "add") == 0) {
			argn += 1;
			argc -= 1;

			while (argc) {
				lib = lib_AddReplace(lib, argv[argn++]);
				argc -= 1;
			}
			lib_Write(lib, libname);
			lib_Free(lib);
		} else if (strcmp(argv[argn], "delete") == 0) {
			argn += 1;
			argc -= 1;

			while (argc) {
				lib =
				    lib_DeleteModule(lib, argv[argn++]);
				argc -= 1;
			}
			lib_Write(lib, libname);
			lib_Free(lib);
		} else if (strcmp(argv[argn], "extract") == 0) {
			argn += 1;
			argc -= 1;

			while (argc) {
				sLibrary *l;

				l = lib_Find(lib, argv[argn]);
				if (l) {
					FILE *f;

					if ((f = fopen(argv[argn], "wb"))) {
						fwrite(l->pData,
						    sizeof(UBYTE),
						    l->nByteLength,
						    f);
						fclose(f);
						printf
						    ("Extracted module '%s'\n",
						    argv[argn]);
					} else
						errx(EX_NOINPUT,
						    "Unable to write module");
				} else
					errx(EX_NOINPUT, "Module not found");

				argn += 1;
				argc -= 1;
			}
			lib_Free(lib);
		} else if (strcmp(argv[argn], "list") == 0) {
			argn += 1;
			argc -= 1;

			sLibrary *l;

			l = lib;

			while (l) {
				printf("%10ld %s\n",
				    l->nByteLength,
				    l->tName);
				l = l->pNext;
			}
		} else
			PrintUsage();
	} else
		PrintUsage();

	return (0);
}
