/*
 * RGBFix : Perform various tasks on a Gameboy image-file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asmotor.h"



/*
 * Option defines
 *
 */

#define OPTF_DEBUG		0x01L
#define OPTF_PAD		0x02L
#define OPTF_VALIDATE	0x04L
#define OPTF_TITLE		0x08L
#define OPTF_TRUNCATE	0x10L

unsigned long ulOptions;




/*
 * Misc. variables
 *
 */

unsigned char NintendoChar[48]=
{
	0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
	0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
	0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E
};




/*
 * Misc. routines
 *
 */

void PrintUsage( void )
{
	printf( "RGBFix v" RGBFIX_VERSION " (part of ASMotor " ASMOTOR_VERSION ")\n\n" );
	printf( "Usage: rgbfix [options] image[.gb]\n" );
	printf( "Options:\n" );
	printf( "\t-h\t\tThis text\n" );
	printf( "\t-d\t\tDebug: Don't change image\n" );
	printf( "\t-p\t\tPad image to valid size\n\t\t\tPads to 32/64/128/256/512kB as appropriate\n" );
	printf( "\t-r\t\ttRuncate image to valid size\n\t\t\tTruncates to 32/64/128/256/512kB as appropriate\n" );
	printf( "\t-t<name>\tChange cartridge title field (16 characters)\n" );
	printf( "\t-v\t\tValidate header\n\t\t\tCorrects - Nintendo Character Area (0x0104)\n\t\t\t\t - ROM type (0x0147)\n\t\t\t\t - ROM size (0x0148)\n\t\t\t\t - Checksums (0x014D-0x014F)\n" );
	exit( 0 );
}

void FatalError( char *s )
{
	printf( "\n***ERROR: %s\n\n", s );
	PrintUsage();
}

long int FileSize( FILE *f )
{
	long prevpos;
	long r;

	fflush( f );
	prevpos=ftell( f );
	fseek( f, 0, SEEK_END );
	r=ftell( f );
	fseek( f, prevpos, SEEK_SET );
	return( r );
}

int FileExists( char *s )
{
	FILE *f;

	if( (f=fopen(s,"rb"))!=NULL )
	{
		fclose( f );
		return( 1 );
	}
	else
		return( 0 );
}

/*
 * Das main
 *
 */

int main( int argc, char *argv[] )
{
	int argn=1;
	char filename[512];
	char cartname[32];
	FILE *f;

	ulOptions=0;

	if( (--argc)==0 )
		PrintUsage();

	while( *argv[argn]=='-' )
	{
		argc-=1;
		switch( argv[argn++][1] )
		{
			case '?':
			case 'h':
				PrintUsage();
				break;
			case 'd':
				ulOptions|=OPTF_DEBUG;
				break;
			case 'p':
				ulOptions|=OPTF_PAD;
				break;
			case 'r':
				ulOptions|=OPTF_TRUNCATE;
				break;
			case 'v':
				ulOptions|=OPTF_VALIDATE;
				break;
			case 't':
				strncpy( cartname, argv[argn-1]+2, 16 );
				ulOptions|=OPTF_TITLE;
				break;
		}
	}

	strcpy( filename, argv[argn++] );

	if( !FileExists(filename) )
		strcat( filename, ".gb" );

	if( (f=fopen(filename,"rb+"))!=NULL )
	{
		/*
		 * -d (Debug) option code
		 *
		 */

		if( ulOptions&OPTF_DEBUG )
		{
			printf( "-d (Debug) option enabled...\n" );
		}

		/*
		 * -p (Pad) option code
		 *
		 */

		if( ulOptions&OPTF_PAD )
		{
			long size, padto;
			long bytesadded=0;

			size=FileSize( f );
			padto=0x8000L;
			while( size>padto )
				padto*=2;

			printf( "Padding to %ldkB:\n", padto/1024 );

/*
			if( padto<=0x80000L )
			{
 */
				if( size!=padto )
				{
					fflush( stdout );

					fseek( f, 0, SEEK_END );
					while( size<padto )
					{
						size+=1;
						if( (ulOptions&OPTF_DEBUG)==0 )
							fputc( 0, f );
						bytesadded+=1;
					}
					fflush( f );

					printf( "\tAdded %ld bytes\n", bytesadded );
				}
				else
					printf( "\tNo padding needed\n" );
					/*
			}
			else
				FatalError( "Image size exceeds 512kB" );
					 */
		}

		/*
		 * -r (Truncate) option code
		 *
		 */

		if( ulOptions&OPTF_TRUNCATE )
		{
			long size, padto;
			char tempfile[512];
			FILE *tf;

			size=FileSize( f );
			padto=256*32768;
			while( size<padto )
				padto/=2;

			printf( "Truncating to %ldkB:\n", padto/1024 );

			tmpnam( tempfile );

			if( (ulOptions&OPTF_DEBUG)==0 )
			{
				if( (tf=fopen(tempfile,"wb"))!=NULL )
				{
					fseek( f, 0, SEEK_SET );
					while( padto-- )
					{
						fputc( fgetc(f), tf );
					}
					fclose( f );
					fclose( tf );
					remove( filename );
					rename( tempfile, filename );
					f=fopen( filename, "rb+" );
				}
			}
		}

		/*
		 * -t (Set carttitle) option code
		 *
		 */

		if( ulOptions&OPTF_TITLE )
		{
			printf( "Setting cartridge title:\n" );
			if( (ulOptions&OPTF_DEBUG)==0 )
			{
				fflush( f );
				fseek( f, 0x0134L, SEEK_SET );
				fwrite( cartname, 16, 1, f );
				fflush( f );
			}
			printf( "\tTitle set to %s\n", cartname );
		}

		/*
		 * -v (Validate header) option code
		 *
		 */

		if( ulOptions&OPTF_VALIDATE )
		{
			long i, byteschanged=0;
			long cartromsize, calcromsize=0, filesize;
			long carttype;
			unsigned short cartchecksum=0, calcchecksum=0;
			unsigned char cartcompchecksum=0, calccompchecksum=0;
			int ch;

			printf( "Validating header:\n" );
			fflush( stdout );

			/* Nintendo Character Area */

			fflush( f );
			fseek( f, 0x0104L, SEEK_SET );

			for( i=0; i<48; i+=1 )
			{
				int ch;

				ch=fgetc( f );
				if( ch==EOF )
					ch=0x00;
				if( ch!=NintendoChar[i] )
				{
					byteschanged+=1;

					if( (ulOptions&OPTF_DEBUG)==0 )
					{
						fseek( f, -1, SEEK_CUR );
						fputc( NintendoChar[i], f );
						fflush( f );
					}
				}
			}

			fflush( f );

			if( byteschanged )
				printf( "\tChanged %ld bytes in the Nintendo Character Area\n", byteschanged );
			else
				printf( "\tNintendo Character Area is OK\n" );

			/* ROM size */

			fflush( f );
			fseek( f, 0x0148L, SEEK_SET );
			cartromsize=fgetc( f );
			if( cartromsize==EOF )
				cartromsize=0x00;
			filesize=FileSize( f );
			while( filesize>(0x8000L<<calcromsize) )
				calcromsize+=1;

			if( calcromsize!=cartromsize )
			{
				if( (ulOptions&OPTF_DEBUG)==0 )
				{
					fseek( f, -1, SEEK_CUR );
					fputc( calcromsize, f );
					fflush( f );
				}
				printf( "\tChanged ROM size byte from 0x%02lX (%ldkB) to 0x%02lX (%ldkB)\n",
							cartromsize, (0x8000L<<cartromsize)/1024,
							calcromsize, (0x8000L<<calcromsize)/1024 );
			}
			else
				printf( "\tROM size byte is OK\n" );

			/* Cartridge type */

			fflush( f );
			fseek( f, 0x0147L, SEEK_SET );
			carttype=fgetc( f );
			if( carttype==EOF )
				carttype=0x00;

			if( FileSize(f)>0x8000L )
			{
				/* carttype byte must != 0x00 */
				if( carttype==0x00 )
				{
					if( (ulOptions&OPTF_DEBUG)==0 )
					{
						fseek( f, -1, SEEK_CUR );
			 			fputc( 0x01, f );
						fflush( f );
					}
					printf( "\tCartridge type byte changed to 0x01\n" );
				}
				else
					printf( "\tCartridge type byte is OK\n" );
			}
			else
			{
				/* carttype byte can be anything? */
				printf( "\tCartridge type byte is OK\n" );
			}

			/* Checksum */

			fflush( f );
			fseek( f, 0, SEEK_SET );

			for( i=0; i<(0x8000L<<calcromsize); i+=1 )
			{
				ch=fgetc( f );
				if( ch==EOF )
					ch=0;

				if( i<0x0134L )
					calcchecksum+=ch;
				else if( i<0x014DL )
				{
					calccompchecksum+=ch;
					calcchecksum+=ch;
				}
				else if( i==0x014DL )
					cartcompchecksum=ch;
				else if( i==0x014EL )
					cartchecksum=ch<<8;
				else if( i==0x014FL )
					cartchecksum|=ch;
				else
					calcchecksum+=ch;
			}

			calccompchecksum=0xE7-calccompchecksum;
			calcchecksum+=calccompchecksum;

			if( cartchecksum!=calcchecksum )
			{
				fflush( f );
				fseek( f, 0x014EL, SEEK_SET );
				if( (ulOptions&OPTF_DEBUG)==0 )
				{
					fputc( calcchecksum>>8, f );
					fputc( calcchecksum&0xFF, f );
				}
				fflush( f );
				printf( "\tChecksum changed from 0x%04lX to 0x%04lX\n", (long)cartchecksum, (long)calcchecksum );
			}
			else
				printf( "\tChecksum is OK\n" );


			if( cartcompchecksum!=calccompchecksum )
			{
				fflush( f );
				fseek( f, 0x014DL, SEEK_SET );
				if( (ulOptions&OPTF_DEBUG)==0 )
 					fputc( calccompchecksum, f );
				fflush( f );
				printf( "\tCompChecksum changed from 0x%02lX to 0x%02lX\n", (long)cartcompchecksum, (long)calccompchecksum );
			}
			else
				printf( "\tCompChecksum is OK\n" );

		}
		fclose( f );
	}
	else
	{
		FatalError( "Unable to open file" );
	}

	return( 0 );
}