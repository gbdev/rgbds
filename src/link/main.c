#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern/err.h"
#include "link/object.h"
#include "link/output.h"
#include "link/assign.h"
#include "link/patch.h"
#include "link/mylink.h"
#include "link/mapfile.h"
#include "link/main.h"
#include "link/library.h"

enum eBlockType {
	BLOCK_COMMENT,
	BLOCK_OBJECTS,
	BLOCK_LIBRARIES,
	BLOCK_OUTPUT
};

SLONG options = 0;
SLONG fillchar = 0;
char *smartlinkstartsymbol;

/*
 * Print the usagescreen
 *
 */

static void
usage(void)
{
	printf(
"usage: rgblink [-twd] [-l linkerscript] [-m mapfile] [-n symfile] [-O overlay]\n"
"               [-o outfile] [-p pad_value] [-s symbol] file [...]\n");
	exit(1);
}

/*
 * The main routine
 *
 */

int
main(int argc, char *argv[])
{
	int ch;
	char *ep;

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "l:m:n:o:O:p:s:twd")) != -1) {
		switch (ch) {
		case 'l':
			SetLinkerscriptName(optarg);
			break;
		case 'm':
			SetMapfileName(optarg);
			break;
		case 'n':
			SetSymfileName(optarg);
			break;
		case 'o':
			out_Setname(optarg);
			break;
		case 'O':
			out_SetOverlayname(optarg);
			options |= OPT_OVERLAY;
			break;
		case 'p':
			fillchar = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'p'");
			}
			if (fillchar < 0 || fillchar > 0xFF) {
				fprintf(stderr, "Argument for option 'p' must be between 0 and 0xFF");
				exit(1);
			}
			break;
		case 's':
			options |= OPT_SMART_C_LINK;
			smartlinkstartsymbol = optarg;
			break;
		case 't':
			options |= OPT_TINY;
			break;
		case 'd':
			/*
			 * Set to set WRAM as a single continuous block as on
			 * DMG. All WRAM sections must be WRAM0 as bankable WRAM
			 * sections do not exist in this mode. A WRAMX section
			 * will raise an error. VRAM bank 1 can't be used if
			 * this option is enabled either.
			 *
			 * This option implies OPT_CONTWRAM.
			 */
			options |= OPT_DMG_MODE;
			/* fallthrough */
		case 'w':
			/* Set to set WRAM as a single continuous block as on
			 * DMG. All WRAM sections must be WRAM0 as bankable WRAM
			 * sections do not exist in this mode. A WRAMX section
			 * will raise an error. */
			options |= OPT_CONTWRAM;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	for (int i = 0; i < argc; ++i)
		obj_Readfile(argv[i]);

	AddNeededModules();
	AssignSections();
	CreateSymbolTable();
	Patch();
	Output();
	CloseMapfile();

	return (0);
}
