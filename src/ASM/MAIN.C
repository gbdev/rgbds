/*
 * RGBAsm - MAIN.C
 *
 * INCLUDES
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "symbol.h"
#include "fstack.h"
#include "output.h"
#include "main.h"

int     yyparse (void);
void    setuplex (void);

#ifdef	AMIGA
__near long __stack = 65536L;

#endif

/*
 * RGBAsm - MAIN.C
 *
 * VARIABLES
 *
 */

clock_t nStartClock,
        nEndClock;
SLONG	nLineNo;
ULONG	nTotalLines,
        nPass,
        nPC,
        nIFDepth,
        nErrors;

extern int yydebug;

char    temptext[1024];

/*
 * RGBAsm - MAIN.C
 *
 * Option stack
 *
 */

struct	sOptions	DefaultOptions;
struct	sOptions	CurrentOptions;

struct	sOptionStackEntry
{
	struct	sOptions  			Options;
	struct	sOptionStackEntry	*pNext;
};

struct	sOptionStackEntry		*pOptionStack=NULL;

void	opt_SetCurrentOptions( struct sOptions *pOpt )
{
	if( nGBGfxID!=-1 )
	{
		lex_FloatDeleteRange( nGBGfxID, CurrentOptions.gbgfx[0], CurrentOptions.gbgfx[0] );
		lex_FloatDeleteRange( nGBGfxID, CurrentOptions.gbgfx[1], CurrentOptions.gbgfx[1] );
		lex_FloatDeleteRange( nGBGfxID, CurrentOptions.gbgfx[2], CurrentOptions.gbgfx[2] );
		lex_FloatDeleteRange( nGBGfxID, CurrentOptions.gbgfx[3], CurrentOptions.gbgfx[3] );
		lex_FloatDeleteSecondRange( nGBGfxID, CurrentOptions.gbgfx[0], CurrentOptions.gbgfx[0] );
		lex_FloatDeleteSecondRange( nGBGfxID, CurrentOptions.gbgfx[1], CurrentOptions.gbgfx[1] );
		lex_FloatDeleteSecondRange( nGBGfxID, CurrentOptions.gbgfx[2], CurrentOptions.gbgfx[2] );
		lex_FloatDeleteSecondRange( nGBGfxID, CurrentOptions.gbgfx[3], CurrentOptions.gbgfx[3] );
	}

	if( nBinaryID!=-1 )
	{
		lex_FloatDeleteRange( nBinaryID, CurrentOptions.binary[0], CurrentOptions.binary[0] );
		lex_FloatDeleteRange( nBinaryID, CurrentOptions.binary[1], CurrentOptions.binary[1] );
		lex_FloatDeleteSecondRange( nBinaryID, CurrentOptions.binary[0], CurrentOptions.binary[0] );
		lex_FloatDeleteSecondRange( nBinaryID, CurrentOptions.binary[1], CurrentOptions.binary[1] );
	}

	CurrentOptions = *pOpt;

	if( nGBGfxID!=-1 )
	{
		lex_FloatAddRange( nGBGfxID, CurrentOptions.gbgfx[0], CurrentOptions.gbgfx[0] );
		lex_FloatAddRange( nGBGfxID, CurrentOptions.gbgfx[1], CurrentOptions.gbgfx[1] );
		lex_FloatAddRange( nGBGfxID, CurrentOptions.gbgfx[2], CurrentOptions.gbgfx[2] );
		lex_FloatAddRange( nGBGfxID, CurrentOptions.gbgfx[3], CurrentOptions.gbgfx[3] );
		lex_FloatAddSecondRange( nGBGfxID, CurrentOptions.gbgfx[0], CurrentOptions.gbgfx[0] );
		lex_FloatAddSecondRange( nGBGfxID, CurrentOptions.gbgfx[1], CurrentOptions.gbgfx[1] );
		lex_FloatAddSecondRange( nGBGfxID, CurrentOptions.gbgfx[2], CurrentOptions.gbgfx[2] );
		lex_FloatAddSecondRange( nGBGfxID, CurrentOptions.gbgfx[3], CurrentOptions.gbgfx[3] );
	}

	if( nBinaryID!=-1 )
	{
		lex_FloatAddRange( nBinaryID, CurrentOptions.binary[0], CurrentOptions.binary[0] );
		lex_FloatAddRange( nBinaryID, CurrentOptions.binary[1], CurrentOptions.binary[1] );
		lex_FloatAddSecondRange( nBinaryID, CurrentOptions.binary[0], CurrentOptions.binary[0] );
		lex_FloatAddSecondRange( nBinaryID, CurrentOptions.binary[1], CurrentOptions.binary[1] );
	}

}

void	opt_Parse( char *s )
{
	struct	sOptions	newopt;

	newopt=CurrentOptions;

	switch( s[0] )
	{
		case 'e':
			switch( s[1] )
			{
				case 'b':
					newopt.endian=ASM_BIG_ENDIAN;
					printf( "*WARNING*\t :\n\tEndianness forced to BIG for destination CPU\n" );
					break;
				case 'l':
					newopt.endian=ASM_LITTLE_ENDIAN;
					printf( "*WARNING*\t :\n\tEndianness forced to LITTLE for destination CPU\n" );
					break;
				default:
					printf ("*ERROR*\t :\n\tArgument to option -e must be 'b' or 'l'\n" );
					exit (5);
			}
			break;
		case 'g':
			if( strlen(&s[1])==4 )
			{
				newopt.gbgfx[0]=s[1];
				newopt.gbgfx[1]=s[2];
				newopt.gbgfx[2]=s[3];
				newopt.gbgfx[3]=s[4];
			}
			else
			{
				printf ("*ERROR*\t :\n\tMust specify exactly 4 characters for option 'g'\n" );
				exit( 5 );
			}
			break;
		case 'b':
			if( strlen(&s[1])==2 )
			{
				newopt.binary[0]=s[1];
				newopt.binary[1]=s[2];
			}
			else
			{
				printf ("*ERROR*\t :\n\tMust specify exactly 2 characters for option 'b'\n" );
				exit( 5 );
			}
			break;
		case 'z':
			if( strlen(&s[1])<=2 )
			{
				if( strcmp(&s[1],"?")==0 )
				{
					newopt.fillchar=-1;
				}
				else
				{
					int	result;

					result=sscanf( &s[1], "%lx", &newopt.fillchar );
					if( !((result==EOF) || (result==1)) )
					{
						printf ("*ERROR*\t :\n\tInvalid argument for option 'z'\n" );
						exit( 5 );
					}
				}
			}
			else
			{
				printf ("*ERROR*\t :\n\tInvalid argument for option 'z'\n" );
				exit( 5 );
			}
			break;
		default:
			fatalerror( "Unknown option" );
			break;
	}

	opt_SetCurrentOptions( &newopt );
}

void	opt_Push( void )
{
	struct	sOptionStackEntry	*pOpt;

	if( (pOpt=(struct sOptionStackEntry *)malloc(sizeof(struct sOptionStackEntry)))!=NULL )
	{
		pOpt->Options=CurrentOptions;
		pOpt->pNext=pOptionStack;
		pOptionStack=pOpt;
	}
	else
		fatalerror( "No memory for option stack" );
}

void	opt_Pop( void )
{
	if( pOptionStack )
	{
		struct	sOptionStackEntry	*pOpt;

		pOpt=pOptionStack;
		opt_SetCurrentOptions( &(pOpt->Options) );
		pOptionStack=pOpt->pNext;
		free( pOpt );
	}
	else
		fatalerror( "No entries in the option stack" );
}

/*
 * RGBAsm - MAIN.C
 *
 * Error handling
 *
 */

void    yyerror (char *s)
{
    printf ("*ERROR*\t");
    fstk_Dump ();
    printf (" :\n\t%s\n", s);
    nErrors += 1;
}

void    fatalerror (char *s)
{
    yyerror (s);
    exit (5);
}

/*
 * RGBAsm - MAIN.C
 *
 * Help text
 *
 */

void    PrintUsage (void)
{
    printf (APPNAME " v" ASM_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\nUsage: " EXENAME " [options] asmfile\n");
    printf ("Options:\n");
    printf ("\t-h\t\tThis text\n");
    printf ("\t-i<path>\tExtra include path\n");
    printf ("\t-o<file>\tWrite objectoutput to <file>\n");
    printf ("\t-e(l|b)\t\tChange endianness (CAUTION!)\n");
    printf ("\t-g<ASCI>\tChange the four characters used for Gameboy graphics\n"
			"\t\t\tconstants (default is 0123)\n" );
    printf ("\t-b<AS>\t\tChange the two characters used for binary constants\n"
			"\t\t\t(default is 01)\n" );
    printf ("\t-z<hx>\t\tSet the byte value (hex format) used for uninitialised\n"
			"\t\t\tdata (default is ? for random)\n" );
    exit (0);
}

/*
 * RGBAsm - MAIN.C
 *
 * main
 *
 */

int     main (int argc, char *argv[])
{
    char   *tzMainfile;
    int     argn = 1;

    argc -= 1;

    if (argc == 0)
	PrintUsage ();

    /* yydebug=1; */

	DefaultOptions.endian=ASM_DEFAULT_ENDIAN;
	DefaultOptions.gbgfx[0]='0';
	DefaultOptions.gbgfx[1]='1';
	DefaultOptions.gbgfx[2]='2';
	DefaultOptions.gbgfx[3]='3';
	DefaultOptions.binary[0]='0';
	DefaultOptions.binary[1]='1';
	DefaultOptions.fillchar=-1;	//	fill uninitialised data with random values
	opt_SetCurrentOptions( &DefaultOptions );

	while (argv[argn][0] == '-' && argc)
	{
		switch (argv[argn][1])
		{
		    case 'h':
				PrintUsage ();
				break;
		    case 'i':
				fstk_AddIncludePath (&(argv[argn][2]));
				break;
		    case 'o':
				out_SetFileName (&(argv[argn][2]));
				break;
		    case 'e':
			case 'g':
			case 'b':
			case 'z':
				opt_Parse( &argv[argn][1] );
				break;
		    default:
				printf ("*ERROR*\t :\n\tUnknown option '%c'\n", argv[argn][1]);
				exit (5);
				break;
		}
		argn += 1;
		argc -= 1;
    }

	DefaultOptions=CurrentOptions;

    /*tzMainfile=argv[argn++];
     * argc-=1; */
    tzMainfile = argv[argn];

    setuplex ();

    printf ("Assembling %s\n", tzMainfile);

    nStartClock = clock ();

    nLineNo = 1;
    nTotalLines = 0;
    nIFDepth = 0;
    nPC = 0;
    nPass = 1;
    nErrors = 0;
    sym_PrepPass1 ();
    if (fstk_Init (tzMainfile))
    {
		printf ("Pass 1...\n");

		yy_set_state( LEX_STATE_NORMAL );
		opt_SetCurrentOptions( &DefaultOptions );

		if (yyparse () == 0 && nErrors == 0)
		{
		    if (nIFDepth == 0)
		    {
				nTotalLines = 0;
				nLineNo = 1;
				nIFDepth = 0;
				nPC = 0;
				nPass = 2;
				nErrors = 0;
				sym_PrepPass2 ();
				out_PrepPass2 ();
				fstk_Init (tzMainfile);
				yy_set_state( LEX_STATE_NORMAL );
				opt_SetCurrentOptions( &DefaultOptions );

				printf ("Pass 2...\n");

				if (yyparse () == 0 && nErrors == 0)
				{
				    double  timespent;

				    nEndClock = clock ();
				    timespent = ((double) (nEndClock - nStartClock)) / (double) CLOCKS_PER_SEC;
				    printf ("Success! %ld lines in %d.%02d seconds ", nTotalLines, (int) timespent, ((int) (timespent * 100.0)) % 100);
				    if (timespent == 0)
						printf ("(INFINITY lines/minute)\n");
				    else
						printf ("(%d lines/minute)\n", (int) (60 / timespent * nTotalLines));
				    out_WriteObject ();
				}
				else
				{
				    printf ("Assembly aborted in pass 2 (%ld errors)!\n", nErrors);
				    //sym_PrintSymbolTable();
				    exit (5);
				}
		    }
		    else
		    {
				printf ("*ERROR*\t:\tUnterminated IF construct (%ld levels)!\n", nIFDepth);
				exit (5);
		    }
		}
		else
		{
		    printf ("Assembly aborted in pass 1 (%ld errors)!\n", nErrors);
		    exit (5);
		}
    }
    else
    {
		printf ("File '%s' not found\n", tzMainfile);
		exit (5);
    }
    return (0);
}