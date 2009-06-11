#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#include	"asmotor.h"
#include	"types.h"
#include	"library.h"

//	Quick and dirty...but it works
#ifdef __GNUC__
#define	strcmpi	strcasecmp
#endif

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
	printf( "xLib v" LIB_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\n"
			"Usage: xlib library command [module1 module2 ... modulen]\n"
			"Commands:\n\ta\tAdd/replace modules to library\n"
			"\td\tDelete modules from library\n"
			"\tl\tList library contents\n"
			"\tx\tExtract modules from library\n" );
	exit( 0 );
}

/*
 * The main routine
 *
 */

int	main( int argc, char *argv[] )
{
	SLONG	argn=0;
	char	*libname;

	argc-=1;
	argn+=1;

	if( argc>=2 )
	{
		UBYTE		command;
		sLibrary	*lib;

		lib=lib_Read( libname=argv[argn++] );
		argc-=1;

		if( strlen(argv[argn])==1 )
		{
			command=argv[argn++][0];
			argc-=1;

			switch( tolower(command) )
			{
				case	'a':
					while( argc )
					{
						lib=lib_AddReplace( lib, argv[argn++] );
						argc-=1;
					}
					lib_Write( lib, libname );
					lib_Free( lib );
					break;
				case	'd':
					while( argc )
					{
						lib=lib_DeleteModule( lib, argv[argn++] );
						argc-=1;
					}
					lib_Write( lib, libname );
					lib_Free( lib );
					break;
				case	'l':
					{
						sLibrary	*l;

						l=lib;

						while( l )
						{
							printf( "%10d %s\n", l->nByteLength, l->tName );
							l=l->pNext;
						}
					}
					break;
				case	'x':
					while( argc )
					{
						sLibrary	*l;

						l=lib_Find( lib, argv[argn] );
						if( l )
						{
							FILE	*f;

							if( f=fopen(argv[argn],"wb") )
							{
								fwrite( l->pData, sizeof(UBYTE), l->nByteLength, f );
								fclose( f );
								printf( "Extracted module '%s'\n", argv[argn] );
							}
							else
								fatalerror( "Unable to write module" );
						}
						else
							fatalerror( "Module not found" );

						argn+=1;
						argc-=1;
					}
					lib_Free( lib );
					break;
				default:
					fatalerror( "Invalid command" );
					break;
			}

		}
		else
		{
			fatalerror( "Invalid command" );
		}
	}
	else
		PrintUsage();

	return( 0 );
}