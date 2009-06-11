#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"types.h"
#include	"libwrap.h"

extern	void	fatalerror( char *s );

SLONG	file_Length( FILE *f )
{
	ULONG	r,
			p;

	p=ftell( f );
	fseek( f, 0, SEEK_END );
	r=ftell( f );
	fseek( f, p, SEEK_SET );

	return( r );
}

SLONG	file_ReadASCIIz( char *b, FILE *f )
{
	SLONG	r=0;

	while( (*b++ = fgetc(f))!=0 )
		r+=1;

	return( r+1 );
}

void	file_WriteASCIIz( char *b, FILE *f )
{
	while( *b )
		fputc(*b++,f);

	fputc( 0, f );
}

UWORD	file_ReadWord( FILE *f )
{
	UWORD	r;

	r =fgetc( f );
	r|=fgetc( f )<<8;

	return( r );
}

void	file_WriteWord( UWORD w, FILE *f )
{
	fputc( w, f );
	fputc( w>>8, f );
}

ULONG	file_ReadLong( FILE *f )
{
	ULONG	r;

	r =fgetc( f );
	r|=fgetc( f )<<8;
	r|=fgetc( f )<<16;
	r|=fgetc( f )<<24;

	return( r );
}

void	file_WriteLong( UWORD w, FILE *f )
{
	fputc( w, f );
	fputc( w>>8, f );
	fputc( w>>16, f );
	fputc( w>>24, f );
}

sLibrary	*lib_ReadLib0( FILE *f, SLONG size )
{
	if( size )
	{
		sLibrary	*l=NULL,
					*first=NULL;

		while( size>0 )
		{
			if( l==NULL )
			{
				if( (l=(sLibrary *)malloc(sizeof(sLibrary)))==NULL )
					fatalerror( "Out of memory" );

				first=l;
			}
			else
			{
				if( (l->pNext=(sLibrary *)malloc(sizeof(sLibrary)))==NULL )
					fatalerror( "Out of memory" );
				l=l->pNext;
			}

			size-=file_ReadASCIIz( l->tName, f );
			l->uwTime=file_ReadWord( f ); size-=2;
			l->uwDate=file_ReadWord( f ); size-=2;
			l->nByteLength=file_ReadLong( f ); size-=4;
			if( l->pData=(UBYTE *)malloc(l->nByteLength) )
			{
				fread( l->pData, sizeof(UBYTE), l->nByteLength, f );
				size-=l->nByteLength;
			}
			else
				fatalerror( "Out of memory" );

			l->pNext=NULL;
		}
		return( first );
	}

	return( NULL );
}

sLibrary	*lib_Read( char *filename )
{
	FILE		*f;

	if( f=fopen(filename,"rb") )
	{
		SLONG		size;
		char		ID[5];

		size=file_Length( f );
		if( size==0 )
		{
			fclose( f );
			return( NULL );
		}

		fread( ID, sizeof(char), 4, f );
		ID[4]=0;
		size-=4;

		if( strcmp(ID,"XLB0")==0 )
		{
			sLibrary	*r;

			r=lib_ReadLib0( f, size );
			fclose( f );
			printf( "Library '%s' opened\n", filename );
			return( r );
		}
		else
		{
			fclose( f );
			fatalerror( "Not a valid xLib library" );
			return( NULL );
		}
	}
	else
	{
		printf( "Library '%s' not found, it will be created if necessary\n", filename );
		return( NULL );
	}
}

BBOOL	lib_Write( sLibrary *lib, char *filename )
{
	FILE	*f;

	if( f=fopen(filename,"wb") )
	{
		fwrite( "XLB0", sizeof(char), 4, f );
		while( lib )
		{
			file_WriteASCIIz( lib->tName, f );
			file_WriteWord( lib->uwTime, f );
			file_WriteWord( lib->uwDate, f );
			file_WriteLong( lib->nByteLength, f );
			fwrite( lib->pData, sizeof(UBYTE), lib->nByteLength,f );
			lib=lib->pNext;
		}

		fclose( f );
		printf( "Library '%s' closed\n", filename );
		return( 1 );
	}

	return( 0 );
}

void	TruncateFileName( char *dest, char *src )
{
	SLONG	l;

	l=strlen( src )-1;
	while( (l>=0) && (src[l]!='\\') && (src[l]!='/') )
		l-=1;

	strcpy( dest, &src[l+1] );
}

sLibrary	*lib_Find( sLibrary *lib, char *filename )
{
	char	truncname[MAXNAMELENGTH];

	TruncateFileName( truncname, filename );

	while( lib )
	{
		if( strcmp(lib->tName,truncname)==0 )
			break;

		lib=lib->pNext;
	}

	return( lib );
}

sLibrary	*lib_AddReplace( sLibrary *lib, char *filename )
{
	FILE	*f;

	if( f=fopen(filename,"rb") )
	{
		sLibrary	*module;
		char		truncname[MAXNAMELENGTH];

		TruncateFileName( truncname, filename );

		if( (module=lib_Find(lib,filename))==NULL )
		{
			if( module=(sLibrary *)malloc(sizeof(sLibrary)) )
			{
				module->pNext=lib;
				lib=module;
			}
			else
				fatalerror( "Out of memory" );
		}
		else
		{
			/* Module already exists */
			free( module->pData );
		}

		module->nByteLength=file_Length( f );
		strcpy( module->tName, truncname );
		if( module->pData=(UBYTE *)malloc(module->nByteLength) )
		{
			fread( module->pData, sizeof(UBYTE), module->nByteLength, f );
		}

		printf( "Added module '%s'\n", truncname );

		fclose( f );
	}

	return( lib );
}

sLibrary	*lib_DeleteModule( sLibrary *lib, char *filename )
{
	char		truncname[MAXNAMELENGTH];
	sLibrary	**pp,
				**first;
	BBOOL		found=0;

	pp=&lib;
	first=pp;

	TruncateFileName( truncname, filename );
	while( (*pp) && (!found) )
	{
		if( strcmp((*pp)->tName,truncname)==0 )
		{
			sLibrary	*t;

			t=*pp;

			if( t->pData )
				free( t->pData );

			*pp = t->pNext;

			free( t );
			found=1;
		}
		pp=&((*pp)->pNext);
	}

	if( !found )
		fatalerror( "Module not found" );
	else
		printf( "Module '%s' deleted from library\n", truncname );

	return( *first );
}

void	lib_Free( sLibrary *lib )
{
	while( lib )
	{
		sLibrary	*l;

		if( lib->pData )
			free( lib->pData );

		l=lib;
		lib=lib->pNext;
		free( l );
	}
}