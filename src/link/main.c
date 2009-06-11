#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"object.h"
#include	"output.h"
#include	"assign.h"
#include	"patch.h"
#include	"asmotor.h"
#include	"mylink.h"
#include	"mapfile.h"
#include	"main.h"
#include	"library.h"

//	Quick and dirty...but it works
#ifdef __GNUC__
#define	strcmpi	strcasecmp
#endif

enum	eBlockType
{
	BLOCK_COMMENT,
	BLOCK_OBJECTS,
	BLOCK_LIBRARIES,
	BLOCK_OUTPUT
};

SLONG	options=0;
SLONG	fillchar=-1;
enum	eOutputType	outputtype=OUTPUT_GBROM;
char	temptext[1024];
char	smartlinkstartsymbol[256];

/*
 * Print out an errormessage
 *
 */

void	fatalerror( char *s )
{
	printf( "*ERROR* : %s\n", s );
	exit( 5 );
}

/*
 * Print the usagescreen
 *
 */

void	PrintUsage( void )
{
	printf( "xLink v" LINK_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\n"
			"Usage: xlink [options] linkfile\n"
			"Options:\n\t-h\t\tThis text\n"
			"\t-m<mapfile>\tWrite a mapfile\n"
			"\t-n<symfile>\tWrite a NO$GMB compatible symfile\n"
    		"\t-z<hx>\t\tSet the byte value (hex format) used for uninitialised\n"
			"\t\t\tdata (default is ? for random)\n"
    		"\t-s<symbol>\tPerform smart linking starting with <symbol>\n"
			"\t-t\t\tOutput target\n"
			"\t\t-tg\tGameboy ROM image(default)\n"
			"\t\t-ts\tGameboy small mode (32kB)\n"
			"\t\t-tp\tPsion2 reloc module\n" );
	exit( 0 );
}

/*
 * Parse the linkfile and load all the objectfiles
 *
 */

void	ProcessLinkfile( char *tzLinkfile )
{
	FILE	*pLinkfile;
	enum	eBlockType	CurrentBlock=BLOCK_COMMENT;

	if( pLinkfile=fopen(tzLinkfile,"rt") )
	{
		while( !feof(pLinkfile) )
		{
			char	tzLine[256];

			fscanf( pLinkfile, "%s\n", tzLine );
			if( tzLine[0]!='#' )
			{
				if( tzLine[0]=='[' && tzLine[strlen(tzLine)-1]==']' )
				{
					if( strcmpi("[objects]",tzLine)==0 )
						CurrentBlock=BLOCK_OBJECTS;
					else if( strcmpi("[output]",tzLine)==0 )
						CurrentBlock=BLOCK_OUTPUT;
					else if( strcmpi("[libraries]",tzLine)==0 )
						CurrentBlock=BLOCK_LIBRARIES;
					else if( strcmpi("[comment]",tzLine)==0 )
						CurrentBlock=BLOCK_COMMENT;
					else
					{
						fclose( pLinkfile );
						sprintf( temptext, "Unknown block '%s'\n", tzLine );
						fatalerror( temptext );
					}
				}
				else
				{
					switch( CurrentBlock )
					{
						case BLOCK_COMMENT:
							break;
						case BLOCK_OBJECTS:
							obj_Readfile( tzLine );
							break;
						case BLOCK_LIBRARIES:
							lib_Readfile( tzLine );
							break;
						case BLOCK_OUTPUT:
							out_Setname( tzLine );
							break;
					}
				}
			}
		}
		fclose( pLinkfile );
	}
	else
	{
		sprintf( temptext, "Unable to find linkfile '%s'\n", tzLinkfile );
		fatalerror( temptext );
	}

}

/*
 * The main routine
 *
 */

int	main( int argc, char *argv[] )
{
	SLONG	argn=0;

	argc-=1;
	argn+=1;

	if( argc==0 )
		PrintUsage();

	while( *argv[argn]=='-' )
	{
		char opt;
		argc-=1;
		switch( opt=argv[argn++][1] )
		{
			case '?':
			case 'h':
				PrintUsage();
				break;
			case 'm':
				SetMapfileName( argv[argn-1]+2 );
				break;
			case 'n':
				SetSymfileName( argv[argn-1]+2 );
				break;
			case 't':
				switch( opt=argv[argn-1][2] )
				{
					case 'g':
						outputtype=OUTPUT_GBROM;
						break;
					case 's':
						outputtype=OUTPUT_GBROM;
						options|=OPT_SMALL;
						break;
					case 'p':
						outputtype=OUTPUT_PSION2;
						break;
					default:
						sprintf( temptext, "Unknown option 't%c'\n", opt );
						fatalerror( temptext );
						break;
				}
				break;
			case 'z':
				if( strlen(argv[argn-1]+2)<=2 )
				{
					if( strcmp(argv[argn-1]+2,"?")==0 )
					{
						fillchar=-1;
					}
					else
					{
						int	result;

						result=sscanf( argv[argn-1]+2, "%x", &fillchar );
						if( !((result==EOF) || (result==1)) )
						{
							fatalerror("Invalid argument for option 'z'\n" );
						}
					}
				}
				else
				{
					fatalerror("Invalid argument for option 'z'\n" );
				}
				break;
			case 's':
				options|=OPT_SMART_C_LINK;
				strcpy( smartlinkstartsymbol, argv[argn-1]+2 );
				break;
			default:
				sprintf( temptext, "Unknown option '%c'\n", opt );
				fatalerror( temptext );
				break;
		}
	}

	if( argc==1 )
	{
		ProcessLinkfile( argv[argn++] );
		AddNeededModules();
		AssignSections();
		CreateSymbolTable();
		Patch();
		Output();
		CloseMapfile();
	}
	else
		PrintUsage();

	return( 0 );
}