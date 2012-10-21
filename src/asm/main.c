/*
 * RGBAsm - MAIN.C
 *
 * INCLUDES
 *
 */

#include <math.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "asm/symbol.h"
#include "asm/fstack.h"
#include "asm/output.h"
#include "asm/main.h"

int yyparse(void);
void setuplex(void);

#ifdef AMIGA
__near long __stack = 65536L;

#endif

/*
 * RGBAsm - MAIN.C
 *
 * VARIABLES
 *
 */

clock_t nStartClock, nEndClock;
SLONG nLineNo;
ULONG nTotalLines, nPass, nPC, nIFDepth, nErrors;

extern int yydebug;

char temptext[1024];

/*
 * RGBAsm - MAIN.C
 *
 * Option stack
 *
 */

struct sOptions DefaultOptions;
struct sOptions CurrentOptions;

struct sOptionStackEntry {
	struct sOptions Options;
	struct sOptionStackEntry *pNext;
};

struct sOptionStackEntry *pOptionStack = NULL;

void 
opt_SetCurrentOptions(struct sOptions * pOpt)
{
	if (nGBGfxID != -1) {
		lex_FloatDeleteRange(nGBGfxID, CurrentOptions.gbgfx[0],
		    CurrentOptions.gbgfx[0]);
		lex_FloatDeleteRange(nGBGfxID, CurrentOptions.gbgfx[1],
		    CurrentOptions.gbgfx[1]);
		lex_FloatDeleteRange(nGBGfxID, CurrentOptions.gbgfx[2],
		    CurrentOptions.gbgfx[2]);
		lex_FloatDeleteRange(nGBGfxID, CurrentOptions.gbgfx[3],
		    CurrentOptions.gbgfx[3]);
		lex_FloatDeleteSecondRange(nGBGfxID, CurrentOptions.gbgfx[0],
		    CurrentOptions.gbgfx[0]);
		lex_FloatDeleteSecondRange(nGBGfxID, CurrentOptions.gbgfx[1],
		    CurrentOptions.gbgfx[1]);
		lex_FloatDeleteSecondRange(nGBGfxID, CurrentOptions.gbgfx[2],
		    CurrentOptions.gbgfx[2]);
		lex_FloatDeleteSecondRange(nGBGfxID, CurrentOptions.gbgfx[3],
		    CurrentOptions.gbgfx[3]);
	}
	if (nBinaryID != -1) {
		lex_FloatDeleteRange(nBinaryID, CurrentOptions.binary[0],
		    CurrentOptions.binary[0]);
		lex_FloatDeleteRange(nBinaryID, CurrentOptions.binary[1],
		    CurrentOptions.binary[1]);
		lex_FloatDeleteSecondRange(nBinaryID, CurrentOptions.binary[0],
		    CurrentOptions.binary[0]);
		lex_FloatDeleteSecondRange(nBinaryID, CurrentOptions.binary[1],
		    CurrentOptions.binary[1]);
	}
	CurrentOptions = *pOpt;

	if (nGBGfxID != -1) {
		lex_FloatAddRange(nGBGfxID, CurrentOptions.gbgfx[0],
		    CurrentOptions.gbgfx[0]);
		lex_FloatAddRange(nGBGfxID, CurrentOptions.gbgfx[1],
		    CurrentOptions.gbgfx[1]);
		lex_FloatAddRange(nGBGfxID, CurrentOptions.gbgfx[2],
		    CurrentOptions.gbgfx[2]);
		lex_FloatAddRange(nGBGfxID, CurrentOptions.gbgfx[3],
		    CurrentOptions.gbgfx[3]);
		lex_FloatAddSecondRange(nGBGfxID, CurrentOptions.gbgfx[0],
		    CurrentOptions.gbgfx[0]);
		lex_FloatAddSecondRange(nGBGfxID, CurrentOptions.gbgfx[1],
		    CurrentOptions.gbgfx[1]);
		lex_FloatAddSecondRange(nGBGfxID, CurrentOptions.gbgfx[2],
		    CurrentOptions.gbgfx[2]);
		lex_FloatAddSecondRange(nGBGfxID, CurrentOptions.gbgfx[3],
		    CurrentOptions.gbgfx[3]);
	}
	if (nBinaryID != -1) {
		lex_FloatAddRange(nBinaryID, CurrentOptions.binary[0],
		    CurrentOptions.binary[0]);
		lex_FloatAddRange(nBinaryID, CurrentOptions.binary[1],
		    CurrentOptions.binary[1]);
		lex_FloatAddSecondRange(nBinaryID, CurrentOptions.binary[0],
		    CurrentOptions.binary[0]);
		lex_FloatAddSecondRange(nBinaryID, CurrentOptions.binary[1],
		    CurrentOptions.binary[1]);
	}
}

void 
opt_Parse(char *s)
{
	struct sOptions newopt;

	newopt = CurrentOptions;

	switch (s[0]) {
	case 'g':
		if (strlen(&s[1]) == 4) {
			newopt.gbgfx[0] = s[1];
			newopt.gbgfx[1] = s[2];
			newopt.gbgfx[2] = s[3];
			newopt.gbgfx[3] = s[4];
		} else {
			fprintf(stderr, "Must specify exactly 4 characters "
			    "for option 'g'\n");
			exit(1);
		}
		break;
	case 'b':
		if (strlen(&s[1]) == 2) {
			newopt.binary[0] = s[1];
			newopt.binary[1] = s[2];
		} else {
			fprintf(stderr, "Must specify exactly 2 characters "
			    "for option 'b'\n");
			exit(1);
		}
		break;
	case 'z':
		if (strlen(&s[1]) <= 2) {
			int result;

			result = sscanf(&s[1], "%lx", &newopt.fillchar);
			if (!((result == EOF) || (result == 1))) {
				fprintf(stderr,
				    "Invalid argument for option 'z'\n");
				exit(1);
			}
		} else {
			fprintf(stderr, "Invalid argument for option 'z'\n");
			exit(1);
		}
		break;
	default:
		fatalerror("Unknown option");
		break;
	}

	opt_SetCurrentOptions(&newopt);
}

void 
opt_Push(void)
{
	struct sOptionStackEntry *pOpt;

	if ((pOpt =
		(struct sOptionStackEntry *)
		malloc(sizeof(struct sOptionStackEntry))) != NULL) {
		pOpt->Options = CurrentOptions;
		pOpt->pNext = pOptionStack;
		pOptionStack = pOpt;
	} else
		fatalerror("No memory for option stack");
}

void 
opt_Pop(void)
{
	if (pOptionStack) {
		struct sOptionStackEntry *pOpt;

		pOpt = pOptionStack;
		opt_SetCurrentOptions(&(pOpt->Options));
		pOptionStack = pOpt->pNext;
		free(pOpt);
	} else
		fatalerror("No entries in the option stack");
}
/*
 * RGBAsm - MAIN.C
 *
 * Error handling
 *
 */

void 
yyerror(char *s)
{
	printf("*ERROR*\t");
	fstk_Dump();
	printf(" :\n\t%s\n", s);
	nErrors += 1;
}

void 
fatalerror(char *s)
{
	yyerror(s);
	exit(5);
}
/*
 * RGBAsm - MAIN.C
 *
 * Help text
 *
 */

void 
PrintUsage(void)
{
	printf("RGBAsm v" ASM_VERSION " (part of ASMotor " ASMOTOR_VERSION
	    ")\n\n");
	printf("Usage: rgbasm [-b chars] [-g chars] [-i path] [-o outfile] [-p pad_value] file\n");
	exit(1);
}
/*
 * RGBAsm - MAIN.C
 *
 * main
 *
 */

int 
main(int argc, char *argv[])
{
	int ch;
	char *ep;

	struct sOptions newopt;

	char *tzMainfile;

	if (argc == 1)
		PrintUsage();

	/* yydebug=1; */

	DefaultOptions.gbgfx[0] = '0';
	DefaultOptions.gbgfx[1] = '1';
	DefaultOptions.gbgfx[2] = '2';
	DefaultOptions.gbgfx[3] = '3';
	DefaultOptions.binary[0] = '0';
	DefaultOptions.binary[1] = '1';
	DefaultOptions.fillchar = 0;

	opt_SetCurrentOptions(&DefaultOptions);

	newopt = CurrentOptions;

	while ((ch = getopt(argc, argv, "b:g:i:o:p:")) != -1) {
		switch (ch) {
		case 'b':
			if (strlen(optarg) == 2) {
				newopt.binary[0] = optarg[1];
				newopt.binary[1] = optarg[2];
			} else {
				fprintf(stderr, "Must specify exactly "
				    "2 characters for option 'b'\n");
				exit(1);
			}
		case 'g':
			if (strlen(optarg) == 4) {
				newopt.gbgfx[0] = optarg[1];
				newopt.gbgfx[1] = optarg[2];
				newopt.gbgfx[2] = optarg[3];
				newopt.gbgfx[3] = optarg[4];
			} else {
				fprintf(stderr, "Must specify exactly "
				    "4 characters for option 'g'\n");
				exit(1);
			}
			break;
		case 'i':
			fstk_AddIncludePath(optarg);
			break;
		case 'o':
			out_SetFileName(optarg);
			break;
		case 'p':
			newopt.fillchar = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				fprintf(stderr,
				    "Invalid argument for option 'p'\n");
				exit(1);
			}
			if (newopt.fillchar < 0 || newopt.fillchar > 0xFF) {
				fprintf(stderr, "Argument for option 'p' "
				    "must be between 0 and 0xFF\n");
				exit(1);
			}
			break;
		default:
			PrintUsage();
		}
	}
	argc -= optind;
	argv += optind;

	opt_SetCurrentOptions(&newopt);

	DefaultOptions = CurrentOptions;

	/* tzMainfile=argv[argn++]; argc-=1; */
	tzMainfile = argv[argc - 1];

	setuplex();

	printf("Assembling %s\n", tzMainfile);

	nStartClock = clock();

	nLineNo = 1;
	nTotalLines = 0;
	nIFDepth = 0;
	nPC = 0;
	nPass = 1;
	nErrors = 0;
	sym_PrepPass1();
	if (fstk_Init(tzMainfile)) {
		printf("Pass 1...\n");

		yy_set_state(LEX_STATE_NORMAL);
		opt_SetCurrentOptions(&DefaultOptions);

		if (yyparse() == 0 && nErrors == 0) {
			if (nIFDepth == 0) {
				nTotalLines = 0;
				nLineNo = 1;
				nIFDepth = 0;
				nPC = 0;
				nPass = 2;
				nErrors = 0;
				sym_PrepPass2();
				out_PrepPass2();
				fstk_Init(tzMainfile);
				yy_set_state(LEX_STATE_NORMAL);
				opt_SetCurrentOptions(&DefaultOptions);

				printf("Pass 2...\n");

				if (yyparse() == 0 && nErrors == 0) {
					double timespent;

					nEndClock = clock();
					timespent =
					    ((double) (nEndClock - nStartClock))
					    / (double) CLOCKS_PER_SEC;
					printf
					    ("Success! %ld lines in %d.%02d seconds ",
					    nTotalLines, (int) timespent,
					    ((int) (timespent * 100.0)) % 100);
					if (timespent == 0)
						printf
						    ("(INFINITY lines/minute)\n");
					else
						printf("(%d lines/minute)\n",
						    (int) (60 / timespent *
							nTotalLines));
					out_WriteObject();
				} else {
					printf
					    ("Assembly aborted in pass 2 (%ld errors)!\n",
					    nErrors);
					//sym_PrintSymbolTable();
					exit(5);
				}
			} else {
				fprintf(stderr,
				    "Unterminated IF construct (%ld levels)!\n",
				    nIFDepth);
				exit(1);
			}
		} else {
			fprintf(stderr,
			    "Assembly aborted in pass 1 (%ld errors)!\n",
			    nErrors);
			exit(1);
		}
	} else {
		printf("File '%s' not found\n", tzMainfile);
		exit(5);
	}
	return (0);
}
