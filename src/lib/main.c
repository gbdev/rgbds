#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asmotor.h"

#include "lib/types.h"
#include "lib/library.h"

/*
 * Print the usagescreen
 *
 */

static void 
usage(void)
{
	printf("RGBLib v" LIB_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\n");
	printf("usage: rgblib file [add | delete | extract | list] [module ...]\n");
	exit(1);
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
					} else {
						fprintf(stderr,
						    "Unable to write module '%s': ", argv[argn]);
						perror(NULL);
						exit(1);
					}
				} else {
					fprintf(stderr, "Module not found\n");
					exit(1);
				}

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
			usage();
	} else
		usage();

	return (0);
}
