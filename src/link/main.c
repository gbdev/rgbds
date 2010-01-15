#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asmotor.h"

#include "link/object.h"
#include "link/output.h"
#include "link/assign.h"
#include "link/patch.h"
#include "link/mylink.h"
#include "link/mapfile.h"
#include "link/main.h"
#include "link/library.h"

// Quick and dirty...but it works
#ifdef __GNUC__
#define strcmpi	strcasecmp
#endif

enum eBlockType {
	BLOCK_COMMENT,
	BLOCK_OBJECTS,
	BLOCK_LIBRARIES,
	BLOCK_OUTPUT
};

SLONG options = 0;
SLONG fillchar = 0;
enum eOutputType outputtype = OUTPUT_GBROM;
char smartlinkstartsymbol[256];

/*
 * Print the usagescreen
 *
 */

static void 
usage(void)
{
	printf("xLink v" LINK_VERSION " (part of ASMotor " ASMOTOR_VERSION
	    ")\n\n");
	printf("usage: xlink [m mapfile] [-n symfile] [-s symbol] [-tg | -ts | -tp]\n");
	printf("\t    [-z pad_value] linkfile\n");

	exit(0);
}
/*
 * Parse the linkfile and load all the objectfiles
 *
 */

void 
ProcessLinkfile(char *tzLinkfile)
{
	FILE *pLinkfile;
	enum eBlockType CurrentBlock = BLOCK_COMMENT;

	pLinkfile = fopen(tzLinkfile, "rt");
	if (!pLinkfile) {
		errx(5, "Unable to find linkfile '%s'", tzLinkfile);
	}
	while (!feof(pLinkfile)) {
		char tzLine[256];

		fscanf(pLinkfile, "%s\n", tzLine);
		if (tzLine[0] != '#') {
			if (tzLine[0] == '['
			    && tzLine[strlen(tzLine) - 1] == ']') {
				if (strcmpi("[objects]", tzLine) == 0)
					CurrentBlock = BLOCK_OBJECTS;
				else if (strcmpi("[output]", tzLine) ==
				    0)
					CurrentBlock = BLOCK_OUTPUT;
				else if (strcmpi("[libraries]", tzLine)
				    == 0)
					CurrentBlock = BLOCK_LIBRARIES;
				else if (strcmpi("[comment]", tzLine) ==
				    0)
					CurrentBlock = BLOCK_COMMENT;
				else {
					fclose(pLinkfile);
					errx(5, 
					    "Unknown block '%s'",
					    tzLine);
				}
			} else {
				switch (CurrentBlock) {
				case BLOCK_COMMENT:
					break;
				case BLOCK_OBJECTS:
					obj_Readfile(tzLine);
					break;
				case BLOCK_LIBRARIES:
					lib_Readfile(tzLine);
					break;
				case BLOCK_OUTPUT:
					out_Setname(tzLine);
					break;
				}
			}
		}
	}

	fclose(pLinkfile);
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

	SLONG argn = 0;

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "m:n:s:t:z:")) != -1) {
		switch (ch) {
		case 'm':
			SetMapfileName(optarg);
			break;
		case 'n':
			SetSymfileName(optarg);
			break;
		case 's':
			options |= OPT_SMART_C_LINK;
			strcpy(smartlinkstartsymbol, optarg);
			break;
		case 't':
			switch (optarg[0]) {
			case 'g':
				outputtype = OUTPUT_GBROM;
				break;
			case 's':
				outputtype = OUTPUT_GBROM;
				options |= OPT_SMALL;
				break;
			case 'p':
				outputtype = OUTPUT_PSION2;
				break;
			default:
				errx(5, "Invalid argument to option t");
				break;
			}
			break;
		case 'z':
			if (optarg[0] == '?')
				fillchar = -1;
			else {
				fillchar = strtoul(optarg, &ep, 0);
				if (optarg[0] == '\0' || *ep != '\0')
					errx(5, "Invalid argument for option 'z'");
				if (fillchar < 0 || fillchar > 0xFF)
					errx(5, "Argument for option 'z' must be between 0 and 0xFF");
			}
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1) {
		ProcessLinkfile(argv[argc - 1]);
		AddNeededModules();
		AssignSections();
		CreateSymbolTable();
		Patch();
		Output();
		CloseMapfile();
	} else
		usage();

	return (0);
}
