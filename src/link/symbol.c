#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"main.h"
#include	"patch.h"
#include	"types.h"

#define HASHSIZE 73

struct ISymbol
{
	char	*pzName;
	SLONG	nValue;
	SLONG	nBank;	//	-1=const
	struct ISymbol	*pNext;
};

struct ISymbol *tHash[HASHSIZE];

SLONG	calchash( char *s )
{
	SLONG	r=0;
	while( *s )
		r+=*s++;

	return( r%HASHSIZE );
}

void sym_Init( void )
{
	SLONG	i;
	for( i=0; i<HASHSIZE; i+=1 )
		tHash[i]=NULL;
}

SLONG	sym_GetValue( char *tzName )
{
	if( strcmp(tzName,"@")==0 )
	{
		return( nPC );
	}
	else
	{
		struct ISymbol **ppSym;

		ppSym=&(tHash[calchash(tzName)]);
		while( *ppSym )
		{
			if( strcmp(tzName,(*ppSym)->pzName) )
			{
				ppSym=&((*ppSym)->pNext);
			}
			else
			{
				return( (*ppSym)->nValue );
			}
		}

		sprintf( temptext, "Unknown symbol '%s'", tzName );
		fatalerror( temptext );
		return( 0 );
	}
}

SLONG	sym_GetBank( char *tzName )
{
	struct ISymbol **ppSym;

	ppSym=&(tHash[calchash(tzName)]);
	while( *ppSym )
	{
		if( strcmp(tzName,(*ppSym)->pzName) )
		{
			ppSym=&((*ppSym)->pNext);
		}
		else
		{
			return( (*ppSym)->nBank );
		}
	}

	sprintf( temptext, "Unknown symbol '%s'" );
	fatalerror( temptext );
	return( 0 );
}

void sym_CreateSymbol( char *tzName, SLONG nValue, SBYTE nBank )
{
	if( strcmp(tzName,"@")==0 )
	{
	}
	else
	{
		struct ISymbol **ppSym;

		ppSym=&(tHash[calchash(tzName)]);

		while( *ppSym )
		{
			if( strcmp(tzName,(*ppSym)->pzName) )
			{
				ppSym=&((*ppSym)->pNext);
			}
			else
			{
				if( nBank==-1 )
					return;

				sprintf( temptext, "Symbol '%s' defined more than once\n", tzName );
				fatalerror( temptext );
			}
		}

		if( *ppSym=(struct ISymbol *)malloc(sizeof(struct ISymbol)) )
		{
			if( (*ppSym)->pzName=(char *)malloc(strlen(tzName)+1) )
			{
				strcpy( (*ppSym)->pzName, tzName );
				(*ppSym)->nValue=nValue;
				(*ppSym)->nBank=nBank;
				(*ppSym)->pNext=NULL;
			}
		}
	}
}