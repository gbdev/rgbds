#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
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
	printf("usage: xlink [-t] [-l library] [-m mapfile] [-n symfile] [-o outfile] [-s symbol]\n");
	printf("\t     [-z pad_value] objectfile [...]\n");

	exit(EX_USAGE);
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
		errx(EX_NOINPUT, "Unable to find linkfile '%s'", tzLinkfile);
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

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "l:m:n:o:s:tz:")) != -1) {
		switch (ch) {
		case 'l':
			lib_Readfile(optarg);
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
		case 's':
			options |= OPT_SMART_C_LINK;
			strcpy(smartlinkstartsymbol, optarg);
			break;
		case 't':
			options |= OPT_SMALL;
			break;
		case 'z':
			if (optarg[0] == '?')
				fillchar = -1;
			else {
				fillchar = strtoul(optarg, &ep, 0);
				if (optarg[0] == '\0' || *ep != '\0')
					errx(EX_USAGE, "Invalid argument for option 'z'");
				if (fillchar < 0 || fillchar > 0xFF)
					errx(EX_USAGE, "Argument for option 'z' must be between 0 and 0xFF");
			}
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
