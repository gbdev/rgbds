%{
#include "symbol.h"
#include "asm.h"
#include "output.h"
#include "mylink.h"
#include "fstack.h"
#include "mymath.h"
#include "rpn.h"
#include "main.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include	<ctype.h>

char	*tzNewMacro;
ULONG	ulNewMacroSize;

ULONG	symvaluetostring( char *dest, char *sym )
{
	if( sym_isString(sym) )
		strcpy( dest, sym_GetStringValue(sym) );
	else
		sprintf( dest, "$%lX", sym_GetConstantValue(sym) );

	return( strlen(dest) );
}

ULONG	str2int( char *s )
{
	ULONG r=0;
	while( *s )
	{
		r<<=8;
		r|=(UBYTE)(*s++);
	}
	return( r );
}

ULONG	isWhiteSpace( char s )
{
	return( s==' ' || s=='\t' || s=='\0' || s=='\n' );
}

ULONG	isRept( char *s )
{
	return( (strnicmp(s,"REPT",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

ULONG	isEndr( char *s )
{
	return( (strnicmp(s,"Endr",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

void	copyrept( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;

	while( *src && level )
	{
		if( instring==0 )
		{
			if( isRept(src) )
			{
				level+=1;
				src+=4;
			}
			else if( isEndr(src) )
			{
				level-=1;
				src+=4;
			}
			else
			{
				if( *src=='\"' )
					instring=1;
				src+=1;
			}
		}
		else
		{
			if( *src=='\\' )
			{
				src+=2;
			}
			else if( *src=='\"' )
			{
				src+=1;
				instring=0;
			}
			else
			{
				src+=1;
			}
		}
	}

	len=src-pCurrentBuffer->pBuffer-4;

	src=pCurrentBuffer->pBuffer;
	ulNewMacroSize=len;

	if( (tzNewMacro=(char *)malloc(ulNewMacroSize+1))!=NULL )
	{
		ULONG i;

		tzNewMacro[ulNewMacroSize]=0;
		for( i=0; i<ulNewMacroSize; i+=1 )
		{
			if( (tzNewMacro[i]=src[i])=='\n' )
				nLineNo+=1;
		}
	}
	else
		fatalerror( "No mem for REPT block" );

	yyskipbytes( ulNewMacroSize+4 );

}

ULONG	isMacro( char *s )
{
	return( (strnicmp(s,"MACRO",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[5]) );
}

ULONG	isEndm( char *s )
{
	return( (strnicmp(s,"Endm",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

void	copymacro( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;

	while( *src && level )
	{
		if( instring==0 )
		{
			if( isMacro(src) )
			{
				level+=1;
				src+=4;
			}
			else if( isEndm(src) )
			{
				level-=1;
				src+=4;
			}
			else
			{
				if( *src=='\"' )
					instring=1;
				src+=1;
			}
		}
		else
		{
			if( *src=='\\' )
			{
				src+=2;
			}
			else if( *src=='\"' )
			{
				src+=1;
				instring=0;
			}
			else
			{
				src+=1;
			}
		}
	}

	len=src-pCurrentBuffer->pBuffer-4;

	src=pCurrentBuffer->pBuffer;
	ulNewMacroSize=len;

	if( (tzNewMacro=(char *)malloc(ulNewMacroSize+2))!=NULL )
	{
		ULONG i;

		tzNewMacro[ulNewMacroSize]='\n';
		tzNewMacro[ulNewMacroSize+1]=0;
		for( i=0; i<ulNewMacroSize; i+=1 )
		{
			if( (tzNewMacro[i]=src[i])=='\n' )
				nLineNo+=1;
		}
	}
	else
		fatalerror( "No mem for MACRO definition" );

	yyskipbytes( ulNewMacroSize+4 );
}

ULONG	isIf( char *s )
{
	return( (strnicmp(s,"If",2)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[2]) );
}

ULONG	isElse( char *s )
{
	return( (strnicmp(s,"Else",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

ULONG	isEndc( char *s )
{
	return( (strnicmp(s,"Endc",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

void	if_skip_to_else( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;

	while( *src && level )
	{
		if( *src=='\n' )
			nLineNo+=1;

		if( instring==0 )
		{
			if( isIf(src) )
			{
				level+=1;
				src+=2;
			}
			else if( level==1 && isElse(src) )
			{
				level-=1;
				src+=4;
			}
			else if( isEndc(src) )
			{
				level-=1;
				if( level!=0 )
					src+=4;
			}
			else
			{
				if( *src=='\"' )
					instring=1;
				src+=1;
			}
		}
		else
		{
			if( *src=='\\' )
			{
				src+=2;
			}
			else if( *src=='\"' )
			{
				src+=1;
				instring=0;
			}
			else
			{
				src+=1;
			}
		}
	}

	len=src-pCurrentBuffer->pBuffer;

	yyskipbytes( len );
	yyunput( '\n' );
	nLineNo-=1;
}

void	if_skip_to_endc( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;

	while( *src && level )
	{
		if( *src=='\n' )
			nLineNo+=1;

		if( instring==0 )
		{
			if( isIf(src) )
			{
				level+=1;
				src+=2;
			}
			else if( isEndc(src) )
			{
				level-=1;
				if( level!=0 )
					src+=4;
			}
			else
			{
				if( *src=='\"' )
					instring=1;
				src+=1;
			}
		}
		else
		{
			if( *src=='\\' )
			{
				src+=2;
			}
			else if( *src=='\"' )
			{
				src+=1;
				instring=0;
			}
			else
			{
				src+=1;
			}
		}
	}

	len=src-pCurrentBuffer->pBuffer;

	yyskipbytes( len );
	yyunput( '\n' );
	nLineNo-=1;
}


#ifdef	PCENGINE
ULONG	ZP( struct Expression *expr )
{
	return( (!rpn_isReloc(expr)) && (expr->nVal>0x1FFF) && (expr->nVal<0x2100) );
}

void	out_ZPByte( struct Expression *expr )
{
	if( rpn_isReloc(expr) )
	{
		rpn_CheckZP(expr,expr);
		out_RelByte(expr);
	}
	else
	{
		if( ZP(expr) )
			out_AbsByte(expr->nVal-0x2000);
		else
			fatalerror( "Value not in zeropage");
	}
}
#endif

%}

%union
{
    char tzSym[MAXSYMLEN+1];
    char tzString[256];
    struct Expression sVal;
    SLONG nConstValue;
}

%type	<sVal>	relocconst
%type	<nConstValue>	const
%type	<nConstValue>	const_3bit
%type	<sVal>	const_8bit
%type	<sVal>	const_16bit
%type	<sVal>	const_PCrel
%type	<nConstValue>	sectiontype

%type	<tzString>	string

%token	<nConstValue>		T_NUMBER
%token	<tzString>	T_STRING

%left	T_OP_LOGICNOT
%left	T_OP_LOGICOR T_OP_LOGICAND T_OP_LOGICEQU
%left	T_OP_LOGICGT T_OP_LOGICLT T_OP_LOGICGE T_OP_LOGICLE T_OP_LOGICNE
%left	T_OP_ADD T_OP_SUB
%left	T_OP_OR T_OP_XOR T_OP_AND
%left	T_OP_SHL T_OP_SHR
%left	T_OP_MUL T_OP_DIV T_OP_MOD
%left	T_OP_NOT
%left	T_OP_DEF
%left	T_OP_BANK
%left	T_OP_SIN
%left	T_OP_COS
%left	T_OP_TAN
%left	T_OP_ASIN
%left	T_OP_ACOS
%left	T_OP_ATAN
%left	T_OP_ATAN2
%left	T_OP_FDIV
%left	T_OP_FMUL

%left	T_OP_STRCMP
%left	T_OP_STRIN
%left	T_OP_STRSUB
%left	T_OP_STRLEN
%left	T_OP_STRCAT
%left	T_OP_STRUPR
%left	T_OP_STRLWR

%left	NEG     /* negation--unary minus */

%token	<tzSym> T_LABEL
%token	<tzSym> T_ID
%token	<tzSym> T_POP_EQU
%token	<tzSym> T_POP_SET
%token	<tzSym> T_POP_EQUS

%token	T_POP_INCLUDE T_POP_PRINTF T_POP_PRINTT T_POP_PRINTV T_POP_IF T_POP_ELSE T_POP_ENDC
%token	T_POP_IMPORT T_POP_EXPORT T_POP_GLOBAL
%token	T_POP_DB T_POP_DS T_POP_DW T_POP_DL
%token	T_POP_SECTION
%token	T_POP_RB
%token	T_POP_RW
%token	T_POP_RL
%token	T_POP_MACRO
%token	T_POP_ENDM
%token	T_POP_RSRESET T_POP_RSSET
%token	T_POP_INCBIN T_POP_REPT
%token	T_POP_SHIFT
%token	T_POP_ENDR
%token	T_POP_FAIL
%token	T_POP_WARN
%token	T_POP_PURGE
%token	T_POP_POPS
%token	T_POP_PUSHS
%token	T_POP_POPO
%token	T_POP_PUSHO
%token	T_POP_OPT