%{
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "asm/symbol.h"
#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/output.h"
#include "asm/mylink.h"
#include "asm/fstack.h"
#include "asm/mymath.h"
#include "asm/rpn.h"
#include "asm/main.h"
#include "asm/lexer.h"

char	*tzNewMacro;
ULONG	ulNewMacroSize;

void
bankrangecheck(char *name, ULONG secttype, SLONG org, SLONG bank)
{
	SLONG minbank, maxbank;
	char *stype = NULL;
	switch (secttype) {
	case SECT_ROMX:
		stype = "ROMX";
		minbank = 1;
		maxbank = 0x1ff;
		break;
	case SECT_SRAM:
		stype = "SRAM";
		minbank = 0;
		maxbank = 0x1ff;
		break;
	case SECT_WRAMX:
		stype = "WRAMX";
		minbank = 1;
		maxbank = 7;
		break;
	case SECT_VRAM:
		stype = "VRAM";
		minbank = 0;
		maxbank = 1;
		break;
	default:
		yyerror("BANK only allowed for "
		    "ROMX, WRAMX, SRAM, or VRAM sections");
	}

	if (stype && (bank < minbank || bank > maxbank)) {
		yyerror("%s bank value $%x out of range ($%x to $%x)",
		    stype, bank, minbank, maxbank);
	}

	if (secttype == SECT_WRAMX) {
		bank -= minbank;
	}

	out_NewAbsSection(name, secttype, org, bank);
}

size_t symvaluetostring(char *dest, size_t maxLength, char *sym)
{
	size_t length;

	if (sym_isString(sym)) {
		char *src = sym_GetStringValue(sym);
		size_t i;

		for (i = 0; src[i] != 0; i++) {
			if (i >= maxLength) {
				fatalerror("Symbol value too long to fit buffer");
			}
			dest[i] = src[i];
		}

		length = i;
	} else {
		ULONG value = sym_GetConstantValue(sym);
		int fullLength = snprintf(dest, maxLength + 1, "$%lX", value);

		if (fullLength < 0) {
			fatalerror("snprintf encoding error");
		} else {
			length = (size_t)fullLength;

			if (length > maxLength) {
				fatalerror("Symbol value too long to fit buffer");
			}
		}
	}

	return length;
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

ULONG	str2int2( char *s, int length )
{
	int i;
	ULONG r=0;
	i = (length - 4 < 0 ? 0 : length - 4);
	while(i < length)
	{
		r<<=8;
		r|=(UBYTE)(s[i]);
		i++;
		
	}
	return( r );
}

ULONG	isWhiteSpace( char s )
{
	return( s==' ' || s=='\t' || s=='\0' || s=='\n' );
}

ULONG	isRept( char *s )
{
	return( (strncasecmp(s,"REPT",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

ULONG	isEndr( char *s )
{
	return( (strncasecmp(s,"Endr",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

void	copyrept( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;
	char	*bufferEnd = pCurrentBuffer->pBufferStart + pCurrentBuffer->nBufferSize;

	while( src < bufferEnd && level )
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

	if (level != 0) {
		fatalerror("Unterminated REPT block");
	}

	len=src-pCurrentBuffer->pBuffer-4;

	src=pCurrentBuffer->pBuffer;
	ulNewMacroSize=len;

	if ((tzNewMacro = malloc(ulNewMacroSize + 1)) != NULL) {
		ULONG i;

		tzNewMacro[ulNewMacroSize]=0;
		for( i=0; i<ulNewMacroSize; i+=1 )
		{
			if( (tzNewMacro[i]=src[i])=='\n' )
				nLineNo+=1;
		}
	} else
		fatalerror( "No mem for REPT block" );

	yyskipbytes( ulNewMacroSize+4 );

}

ULONG	isMacro( char *s )
{
	return( (strncasecmp(s,"MACRO",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[5]) );
}

ULONG	isEndm( char *s )
{
	return( (strncasecmp(s,"Endm",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

void	copymacro( void )
{
	SLONG	level=1, len, instring=0;
	char	*src=pCurrentBuffer->pBuffer;
	char	*bufferEnd = pCurrentBuffer->pBufferStart + pCurrentBuffer->nBufferSize;

	while( src < bufferEnd && level )
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

	if (level != 0) {
		fatalerror("Unterminated MACRO definition");
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
	return( (strncasecmp(s,"If",2)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[2]) );
}

ULONG	isElse( char *s )
{
	return( (strncasecmp(s,"Else",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
}

ULONG	isEndc( char *s )
{
	return( (strncasecmp(s,"Endc",4)==0) && isWhiteSpace(*(s-1)) && isWhiteSpace(s[4]) );
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

	if (level != 0) {
		fatalerror("Unterminated IF construct");
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

	if (level != 0) {
		fatalerror("Unterminated IF construct");
	}

	len=src-pCurrentBuffer->pBuffer;

	yyskipbytes( len );
	yyunput( '\n' );
	nLineNo-=1;
}

%}

%union
{
    char tzSym[MAXSYMLEN + 1];
    char tzString[MAXSTRLEN + 1];
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
%left	T_OP_BANK T_OP_ALIGN
%left	T_OP_SIN
%left	T_OP_COS
%left	T_OP_TAN
%left	T_OP_ASIN
%left	T_OP_ACOS
%left	T_OP_ATAN
%left	T_OP_ATAN2
%left	T_OP_FDIV
%left	T_OP_FMUL
%left	T_OP_ROUND
%left	T_OP_CEIL
%left	T_OP_FLOOR


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
%token	T_POP_CHARMAP
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
%token	T_SECT_WRAM0 T_SECT_VRAM T_SECT_ROMX T_SECT_ROM0 T_SECT_HRAM T_SECT_WRAMX T_SECT_SRAM T_SECT_OAM

%token	T_Z80_ADC T_Z80_ADD T_Z80_AND
%token	T_Z80_BIT
%token	T_Z80_CALL T_Z80_CCF T_Z80_CP T_Z80_CPL
%token	T_Z80_DAA T_Z80_DEC T_Z80_DI
%token	T_Z80_EI T_Z80_EX
%token	T_Z80_HALT
%token	T_Z80_INC
%token	T_Z80_JP T_Z80_JR
%token	T_Z80_LD
%token	T_Z80_LDI
%token	T_Z80_LDD
%token	T_Z80_LDIO
%token	T_Z80_NOP
%token	T_Z80_OR
%token	T_Z80_POP T_Z80_PUSH
%token	T_Z80_RES T_Z80_RET T_Z80_RETI T_Z80_RST
%token	T_Z80_RL T_Z80_RLA T_Z80_RLC T_Z80_RLCA
%token	T_Z80_RR T_Z80_RRA T_Z80_RRC T_Z80_RRCA
%token	T_Z80_SBC T_Z80_SCF T_Z80_STOP
%token	T_Z80_SLA T_Z80_SRA T_Z80_SRL T_Z80_SUB T_Z80_SWAP
%token	T_Z80_XOR

%token	T_MODE_A T_MODE_B T_MODE_C T_MODE_C_IND T_MODE_D T_MODE_E T_MODE_H T_MODE_L
%token	T_MODE_AF
%token	T_MODE_BC T_MODE_BC_IND
%token	T_MODE_DE T_MODE_DE_IND
%token	T_MODE_SP T_MODE_SP_IND
%token	T_MODE_HL T_MODE_HL_IND T_MODE_HL_INDDEC T_MODE_HL_INDINC
%token	T_CC_NZ T_CC_Z T_CC_NC

%type	<nConstValue>	reg_r
%type	<nConstValue>	reg_ss
%type	<nConstValue>	reg_rr
%type	<nConstValue>	reg_tt
%type	<nConstValue>	ccode
%type	<sVal>			op_a_n
%type	<nConstValue>	op_a_r
%type	<nConstValue>	op_hl_ss
%type	<sVal>			op_mem_ind
%start asmfile

%%

asmfile : lines;

/* Note: The lexer add '\n' at the end of the input */
lines : /* empty */
	| lines line '\n' {
		nLineNo += 1;
		nTotalLines += 1;
	};

line : label
	| label cpu_command
	| label macro
	| label simple_pseudoop
	| pseudoop;

label : /* empty */
	| T_LABEL {
		if ($1[0] == '.')
			sym_AddLocalReloc($1);
		else
			sym_AddReloc($1);
	} | T_LABEL ':' {
		if ($1[0] == '.')
			sym_AddLocalReloc($1);
		else
			sym_AddReloc($1);
	} | T_LABEL ':' ':' {
		sym_AddReloc($1);
		sym_Export($1);
	};

macro : T_ID {
		yy_set_state(LEX_STATE_MACROARGS);
	} macroargs {
		yy_set_state(LEX_STATE_NORMAL);

		if (!fstk_RunMacro($1)) {
			fatalerror("Macro '%s' not defined", $1);
		}
	};

macroargs : /* empty */
	| macroarg
	| macroarg ',' macroargs;

macroarg : T_STRING {
		sym_AddNewMacroArg($1);
	};

pseudoop : equ
	| set
	| rb
	| rw
	| rl
	| equs
	| macrodef;

simple_pseudoop	:	include
				|	printf
				|	printt
				|	printv
				|	if
				|	else
				|	endc
				|	import
				|	export
				|	global
				|	db
				|	dw
				|	dl
				|	ds
				|	section
				|	rsreset
				|	rsset
				|	incbin
				|	charmap
				|	rept
				|	shift
				|	fail
				|	warn
				|	purge
				|	pops
				|	pushs
				|	popo
				|	pusho
				|	opt;

opt : T_POP_OPT {
		yy_set_state(LEX_STATE_MACROARGS);
	} opt_list {
		yy_set_state(LEX_STATE_NORMAL);
	};

opt_list : opt_list_entry
	| opt_list_entry ',' opt_list;

opt_list_entry : T_STRING {
		opt_Parse($1);
	};

popo : T_POP_POPO {
		opt_Pop();
	};

pusho : T_POP_PUSHO {
		opt_Push();
	};

pops : T_POP_POPS {
		out_PopSection();
	};

pushs : T_POP_PUSHS {
		out_PushSection();
	};

fail : T_POP_FAIL string {
		fatalerror("%s", $2);
	};

warn : T_POP_WARN string {
		yyerror("%s", $2);
	};

shift			:	T_POP_SHIFT
					{ sym_ShiftCurrentMacroArgs(); }
;

rept			:	T_POP_REPT const
					{
						copyrept();
						fstk_RunRept( $2 );
					}
;

macrodef		:	T_LABEL ':' T_POP_MACRO
					{
						copymacro();
						sym_AddMacro($1);
					}
;

equs			:	T_LABEL T_POP_EQUS string
					{ sym_AddString( $1, $3 ); }
;

rsset			:	T_POP_RSSET const
					{ sym_AddSet( "_RS", $2 ); }
;

rsreset			:	T_POP_RSRESET
					{ sym_AddSet( "_RS", 0 ); }
;

rl				:	T_LABEL T_POP_RL const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+4*$3 );
					}
;

rw				:	T_LABEL T_POP_RW const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+2*$3 );
					}
;

rb				:	T_LABEL T_POP_RB const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+$3 );
					}
;

ds				:	T_POP_DS const
					{ out_Skip( $2 ); }
;

db				:	T_POP_DB constlist_8bit
;

dw				:	T_POP_DW constlist_16bit
;

dl				:	T_POP_DL constlist_32bit
;

purge			:	T_POP_PURGE
					{
						oDontExpandStrings = true;
					}
					purge_list
					{
						oDontExpandStrings = false;
					}
;

purge_list		:	purge_list_entry
				|	purge_list_entry ',' purge_list
;

purge_list_entry	:	T_ID	{ sym_Purge($1); }
;

import			:	T_POP_IMPORT import_list
;

import_list		:	import_list_entry
				|	import_list_entry ',' import_list
;

import_list_entry	:	T_ID	{ sym_Import($1); }
;

export			:	T_POP_EXPORT export_list
;

export_list		:	export_list_entry
				|	export_list_entry ',' export_list
;

export_list_entry	:	T_ID	{ sym_Export($1); }
;

global			:	T_POP_GLOBAL global_list
;

global_list		:	global_list_entry
				|	global_list_entry ',' global_list
;

global_list_entry	:	T_ID	{ sym_Global($1); }
;

equ				:	T_LABEL T_POP_EQU const
					{ sym_AddEqu( $1, $3 ); }
;

set				:	T_LABEL T_POP_SET const
					{ sym_AddSet( $1, $3 ); }
;

include			:	T_POP_INCLUDE string
					{
						fstk_RunInclude($2);
					}
;

incbin			:	T_POP_INCBIN string
					{ out_BinaryFile( $2 ); }
				|	T_POP_INCBIN string ',' const ',' const
					{
						out_BinaryFileSlice( $2, $4, $6 );
					}
;

charmap			:	T_POP_CHARMAP string ',' string
					{
						if(charmap_Add($2, $4[0] & 0xFF) == -1)
						{
							fprintf(stderr, "Error parsing charmap. Either you've added too many (%i), or the input character length is too long (%i)' : %s\n", MAXCHARMAPS, CHARMAPLENGTH, strerror(errno));
							yyerror("Error parsing charmap.");
						}
					}
				|	T_POP_CHARMAP string ',' const
					{
						if(charmap_Add($2, $4 & 0xFF) == -1)
						{
							fprintf(stderr, "Error parsing charmap. Either you've added too many (%i), or the input character length is too long (%i)' : %s\n", MAXCHARMAPS, CHARMAPLENGTH, strerror(errno));
							yyerror("Error parsing charmap.");
						}
					}
;

printt			:	T_POP_PRINTT string
					{
						if( nPass==1 )
							printf( "%s", $2 );
					}
;

printv			:	T_POP_PRINTV const
					{
						if( nPass==1 )
							printf( "$%lX", $2 );
					}
;

printf			:	T_POP_PRINTF const
					{
						if( nPass==1 )
							math_Print( $2 );
					}
;

if				:	T_POP_IF const
					{
						nIFDepth+=1;
						if( !$2 )
						{
							if_skip_to_else();	/* will continue parsing just after ELSE or just at ENDC keyword */
						}
					}

else			:	T_POP_ELSE
					{
						if_skip_to_endc();		/* will continue parsing just at ENDC keyword */
					}
;

endc			:	T_POP_ENDC
					{
						nIFDepth-=1;
					}
;

const_3bit		:	const
					{
						if( ($1<0) || ($1>7) )
						{
							yyerror("Immediate value must be 3-bit");
						}
						else
							$$=$1&0x7;
					}
;

constlist_8bit	:	constlist_8bit_entry
				|	constlist_8bit_entry ',' constlist_8bit
;

constlist_8bit_entry	:               { out_Skip( 1 ); }
						|	const_8bit	{ out_RelByte( &$1 ); }
			   			|	string		{ char *s; int length; s = $1; length = charmap_Convert(&s); out_AbsByteGroup(s, length); free(s); }
;

constlist_16bit	:	constlist_16bit_entry
				|	constlist_16bit_entry ',' constlist_16bit
;

constlist_16bit_entry	:  				{ out_Skip( 2 ); }
						|	const_16bit	{ out_RelWord( &$1 ); }
;


constlist_32bit	:	constlist_32bit_entry
				|	constlist_32bit_entry ',' constlist_32bit
;

constlist_32bit_entry	:				{ out_Skip( 4 ); }
						|	relocconst	{ out_RelLong( &$1 ); }
;


const_PCrel		:	relocconst
					{
						$$ = $1;
						if( !rpn_isPCRelative(&$1) )
							yyerror("Expression must be PC-relative");
					}
;

const_8bit		:	relocconst
					{
						if( (!rpn_isReloc(&$1)) && (($1.nVal<-128) || ($1.nVal>255)) )
						{
							yyerror("Expression must be 8-bit");
						}
						$$=$1;
					}
;

const_16bit		:	relocconst
					{
						if( (!rpn_isReloc(&$1)) && (($1.nVal<-32768) || ($1.nVal>65535)) )
						{
							yyerror("Expression must be 16-bit");
						}
						$$=$1;
					}
;


relocconst		:	T_ID
						{ rpn_Symbol(&$$,$1);	$$.nVal = sym_GetValue($1); }
				|	T_NUMBER
						{ rpn_Number(&$$,$1);	$$.nVal = $1; }
				|	string
						{ char *s; int length; ULONG r; s = $1; length = charmap_Convert(&s); r = str2int2(s, length); free(s); rpn_Number(&$$,r); $$.nVal=r; }
				|	T_OP_LOGICNOT relocconst %prec NEG
						{ rpn_LOGNOT(&$$,&$2); }
				|	relocconst T_OP_LOGICOR relocconst
						{ rpn_LOGOR(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICAND relocconst
						{ rpn_LOGAND(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICEQU relocconst
						{ rpn_LOGEQU(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICGT relocconst
						{ rpn_LOGGT(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICLT relocconst
						{ rpn_LOGLT(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICGE relocconst
					 	{ rpn_LOGGE(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICLE relocconst
					 	{ rpn_LOGLE(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICNE relocconst
					 	{ rpn_LOGNE(&$$,&$1,&$3); }
				|	relocconst T_OP_ADD relocconst
						{ rpn_ADD(&$$,&$1,&$3); }
				|	relocconst T_OP_SUB relocconst
						{ rpn_SUB(&$$,&$1,&$3); }
				|	relocconst T_OP_XOR relocconst
						{ rpn_XOR(&$$,&$1,&$3); }
				|	relocconst T_OP_OR relocconst
						{ rpn_OR(&$$,&$1,&$3); }
				|	relocconst T_OP_AND relocconst
						{ rpn_AND(&$$,&$1,&$3); }
				|	relocconst T_OP_SHL relocconst
						{ rpn_SHL(&$$,&$1,&$3); }
				|	relocconst T_OP_SHR relocconst
						{ rpn_SHR(&$$,&$1,&$3); }
				|	relocconst T_OP_MUL relocconst
						{ rpn_MUL(&$$,&$1,&$3); }
				|	relocconst T_OP_DIV relocconst
						{ rpn_DIV(&$$,&$1,&$3); }
				|	relocconst T_OP_MOD relocconst
						{ rpn_MOD(&$$,&$1,&$3); }
				|	T_OP_ADD relocconst %prec NEG
						{ $$ = $2; }
				|	T_OP_SUB relocconst %prec NEG
						{ rpn_UNNEG(&$$,&$2); }
				|	T_OP_NOT relocconst %prec NEG
						{ rpn_UNNOT(&$$,&$2); }
				|	T_OP_BANK '(' T_ID ')'
						{ rpn_Bank(&$$,$3); $$.nVal = 0; }
				|	T_OP_DEF { oDontExpandStrings = true; } '(' T_ID ')'
						{ rpn_Number(&$$,sym_isConstDefined($4)); oDontExpandStrings = false; }
				|	T_OP_ROUND '(' const ')'			{ rpn_Number(&$$,math_Round($3)); }
				|	T_OP_CEIL '(' const ')'			{ rpn_Number(&$$,math_Ceil($3)); }
				|	T_OP_FLOOR '(' const ')'			{ rpn_Number(&$$,math_Floor($3)); }
				|	T_OP_FDIV '(' const ',' const ')'			{ rpn_Number(&$$,math_Div($3,$5)); }
				|	T_OP_FMUL '(' const ',' const ')'			{ rpn_Number(&$$,math_Mul($3,$5)); }
				|	T_OP_SIN '(' const ')'			{ rpn_Number(&$$,math_Sin($3)); }
				|	T_OP_COS '(' const ')'			{ rpn_Number(&$$,math_Cos($3)); }
				|	T_OP_TAN '(' const ')'			{ rpn_Number(&$$,math_Tan($3)); }
				|	T_OP_ASIN '(' const ')'			{ rpn_Number(&$$,math_ASin($3)); }
				|	T_OP_ACOS '(' const ')'			{ rpn_Number(&$$,math_ACos($3)); }
				|	T_OP_ATAN '(' const ')'			{ rpn_Number(&$$,math_ATan($3)); }
				|	T_OP_ATAN2 '(' const ',' const ')'	{ rpn_Number(&$$,math_ATan2($3,$5)); }
				|	T_OP_STRCMP '(' string ',' string ')'	{ rpn_Number(&$$,strcmp($3,$5)); }
				|	T_OP_STRIN '(' string ',' string ')'
					{
						char	*p;
  						if( (p=strstr($3,$5))!=NULL )
						{
							rpn_Number(&$$,p-$3+1);
						}
						else
						{
							rpn_Number(&$$,0);
						}
					}
				|	T_OP_STRLEN '(' string ')'		{ rpn_Number(&$$,strlen($3)); }
				|	'(' relocconst ')'
						{ $$ = $2; }
;

const			:	T_ID							{ $$ = sym_GetConstantValue($1); }
				|	T_NUMBER 						{ $$ = $1; }
				|	string						{ $$ = str2int($1); }
				|	T_OP_LOGICNOT const %prec NEG	{ $$ = !$2; }
				|	const T_OP_LOGICOR const		{ $$ = $1 || $3; }
				|	const T_OP_LOGICAND const		{ $$ = $1 && $3; }
				|	const T_OP_LOGICEQU const		{ $$ = $1 == $3; }
				|	const T_OP_LOGICGT const 		{ $$ = $1 > $3; }
				|	const T_OP_LOGICLT const		{ $$ = $1 < $3; }
				|	const T_OP_LOGICGE const 		{ $$ = $1 >= $3; }
				|	const T_OP_LOGICLE const 		{ $$ = $1 <= $3; }
				|	const T_OP_LOGICNE const 		{ $$ = $1 != $3; }
				|	const T_OP_ADD const			{ $$ = $1 + $3; }
				|	const T_OP_SUB const			{ $$ = $1 - $3; }
				|	T_ID  T_OP_SUB T_ID				{ $$ = sym_GetDefinedValue($1) - sym_GetDefinedValue($3); }
				|	const T_OP_XOR const			{ $$ = $1 ^ $3; }
				|	const T_OP_OR const				{ $$ = $1 | $3; }
				|	const T_OP_AND const			{ $$ = $1 & $3; }
				|	const T_OP_SHL const			{ $$ = $1 << $3; }
				|	const T_OP_SHR const			{ $$ = $1 >> $3; }
				|	const T_OP_MUL const			{ $$ = $1 * $3; }
				|	const T_OP_DIV const			{
	if ($3 == 0)
		fatalerror("division by zero");
	$$ = $1 / $3;
	}
				|	const T_OP_MOD const			{
	if ($3 == 0)
		fatalerror("division by zero");
	$$ = $1 % $3;
	}
				|	T_OP_ADD const %prec NEG		{ $$ = +$2; }
				|	T_OP_SUB const %prec NEG		{ $$ = -$2; }
				|	T_OP_NOT const %prec NEG		{ $$ = 0xFFFFFFFF^$2; }
				|	T_OP_ROUND '(' const ')'		{ $$ = math_Round($3); }
				|	T_OP_CEIL '(' const ')'			{ $$ = math_Ceil($3); }
				|	T_OP_FLOOR '(' const ')'		{ $$ = math_Floor($3); }
				|	T_OP_FDIV '(' const ',' const ')'			{ $$ = math_Div($3,$5); }
				|	T_OP_FMUL '(' const ',' const ')'			{ $$ = math_Mul($3,$5); }
				|	T_OP_SIN '(' const ')'			{ $$ = math_Sin($3); }
				|	T_OP_COS '(' const ')'			{ $$ = math_Cos($3); }
				|	T_OP_TAN '(' const ')'			{ $$ = math_Tan($3); }
				|	T_OP_ASIN '(' const ')'			{ $$ = math_ASin($3); }
				|	T_OP_ACOS '(' const ')'			{ $$ = math_ACos($3); }
				|	T_OP_ATAN '(' const ')'			{ $$ = math_ATan($3); }
				|	T_OP_ATAN2 '(' const ',' const ')'	{ $$ = math_ATan2($3,$5); }
				|	T_OP_DEF { oDontExpandStrings = true; } '(' T_ID ')'	{ $$ = sym_isConstDefined($4); oDontExpandStrings = false; }
				|	T_OP_STRCMP '(' string ',' string ')'	{ $$ = strcmp( $3, $5 ); }
				|	T_OP_STRIN '(' string ',' string ')'
					{
						char	*p;
  						if( (p=strstr($3,$5))!=NULL )
						{
							$$ = p-$3+1;
						}
						else
						{
							$$ = 0;
						}
					}
				|	T_OP_STRLEN '(' string ')'		{ $$ = strlen($3); }
				|	'(' const ')'					{ $$ = $2; }
;

string			:	T_STRING
					{ strcpy($$,$1); }
				|	T_OP_STRSUB '(' string ',' const ',' const ')'
					{ strncpy($$,$3+$5-1,$7); $$[$7]=0; }
				|	T_OP_STRCAT '(' string ',' string ')'
					{ strcpy($$,$3); strcat($$,$5); }
				|	T_OP_STRUPR '(' string ')'
					{ strcpy($$,$3); upperstring($$); }
				|	T_OP_STRLWR '(' string ')'
					{ strcpy($$,$3); lowerstring($$); }
;
section:
		T_POP_SECTION string ',' sectiontype
		{
			out_NewSection($2,$4);
		}
	|	T_POP_SECTION string ',' sectiontype '[' const ']'
		{
			if( $6>=0 && $6<0x10000 )
				out_NewAbsSection($2,$4,$6,-1);
			else
				yyerror("Address $%x not 16-bit", $6);
		}
	|	T_POP_SECTION string ',' sectiontype ',' T_OP_ALIGN '[' const ']'
		{
			out_NewAlignedSection($2, $4, $8, -1);
		}
	|	T_POP_SECTION string ',' sectiontype ',' T_OP_BANK '[' const ']'
		{
			bankrangecheck($2, $4, -1, $8);
		}
	|	T_POP_SECTION string ',' sectiontype '[' const ']' ',' T_OP_BANK '[' const ']'
		{
			if ($6 < 0 || $6 > 0x10000) {
				yyerror("Address $%x not 16-bit", $6);
			}
			bankrangecheck($2, $4, $6, $11);
		}
	|	T_POP_SECTION string ',' sectiontype ',' T_OP_ALIGN '[' const ']' ',' T_OP_BANK '[' const ']'
		{
			out_NewAlignedSection($2, $4, $8, $13);
		}
	|	T_POP_SECTION string ',' sectiontype ',' T_OP_BANK '[' const ']' ',' T_OP_ALIGN '[' const ']'
		{
			out_NewAlignedSection($2, $4, $13, $8);
		}
;

sectiontype:
		T_SECT_WRAM0	{ $$=SECT_WRAM0; }
	|	T_SECT_VRAM	{ $$=SECT_VRAM; }
	|	T_SECT_ROMX	{ $$=SECT_ROMX; }
	|	T_SECT_ROM0	{ $$=SECT_ROM0; }
	|	T_SECT_HRAM	{ $$=SECT_HRAM; }
	|	T_SECT_WRAMX	{ $$=SECT_WRAMX; }
	|	T_SECT_SRAM	{ $$=SECT_SRAM; }
	|	T_SECT_OAM	{ $$=SECT_OAM; }
;


cpu_command		:	z80_adc
				|	z80_add
				|	z80_and
				|	z80_bit
				|	z80_call
				|	z80_ccf
				|	z80_cp
				|	z80_cpl
				|	z80_daa
				|	z80_dec
				|	z80_di
				|	z80_ei
				|	z80_ex
				|	z80_halt
				|	z80_inc
				|	z80_jp
				|	z80_jr
				|	z80_ld
				|	z80_ldd
				|	z80_ldi
				|	z80_ldio
				|	z80_nop
				|	z80_or
				|	z80_pop
				|	z80_push
				|	z80_res
				|	z80_ret
				|	z80_reti
				|	z80_rl
				|	z80_rla
				|	z80_rlc
				|	z80_rlca
				|	z80_rr
				|	z80_rra
				|	z80_rrc
				|	z80_rrca
				|	z80_rst
				|	z80_sbc
				|	z80_scf
				|	z80_set
				|	z80_sla
				|	z80_sra
				|	z80_srl
				|	z80_stop
				|	z80_sub
				|	z80_swap
				|	z80_xor
;

z80_adc			:	T_Z80_ADC op_a_n	{ out_AbsByte(0xCE); out_RelByte(&$2); }
				|	T_Z80_ADC op_a_r	{ out_AbsByte(0x88|$2); }
;

z80_add			:	T_Z80_ADD op_a_n	{ out_AbsByte(0xC6); out_RelByte(&$2); }
				|	T_Z80_ADD op_a_r	{ out_AbsByte(0x80|$2); }
				|	T_Z80_ADD op_hl_ss	{ out_AbsByte(0x09|($2<<4)); }
				|	T_Z80_ADD T_MODE_SP comma const_8bit
					{ out_AbsByte(0xE8); out_RelByte(&$4); }

;

z80_and			:	T_Z80_AND op_a_n	{ out_AbsByte(0xE6); out_RelByte(&$2); }
				|	T_Z80_AND op_a_r	{ out_AbsByte(0xA0|$2); }
;

z80_bit			:	T_Z80_BIT const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x40|($2<<3)|$4); }
;

z80_call		:	T_Z80_CALL const_16bit
					{ out_AbsByte(0xCD); out_RelWord(&$2); }
				|	T_Z80_CALL ccode comma const_16bit
					{ out_AbsByte(0xC4|($2<<3)); out_RelWord(&$4); }
;

z80_ccf			:	T_Z80_CCF
					{ out_AbsByte(0x3F); }
;

z80_cp			:	T_Z80_CP op_a_n	{ out_AbsByte(0xFE); out_RelByte(&$2); }
				|	T_Z80_CP op_a_r	{ out_AbsByte(0xB8|$2); }
;

z80_cpl			:	T_Z80_CPL { out_AbsByte(0x2F); }
;

z80_daa			:	T_Z80_DAA { out_AbsByte(0x27); }
;

z80_dec			:	T_Z80_DEC reg_r
					{ out_AbsByte(0x05|($2<<3)); }
				|	T_Z80_DEC reg_ss
					{ out_AbsByte(0x0B|($2<<4)); }
;

z80_di			:	T_Z80_DI
					{ out_AbsByte(0xF3); }
;

z80_ei			:	T_Z80_EI
					{ out_AbsByte(0xFB); }
;

z80_ex			:	T_Z80_EX T_MODE_HL comma T_MODE_SP_IND
					{ out_AbsByte(0xE3); }
				|	T_Z80_EX T_MODE_SP_IND comma T_MODE_HL
					{ out_AbsByte(0xE3); }
;

z80_halt: T_Z80_HALT
		{
			out_AbsByte(0x76);
			if (CurrentOptions.haltnop) {
				out_AbsByte(0x00);
			}
		}
;

z80_inc			:	T_Z80_INC reg_r
					{ out_AbsByte(0x04|($2<<3)); }
				|	T_Z80_INC reg_ss
					{ out_AbsByte(0x03|($2<<4)); }
;

z80_jp			:	T_Z80_JP const_16bit
					{ out_AbsByte(0xC3); out_RelWord(&$2); }
				|	T_Z80_JP ccode comma const_16bit
					{ out_AbsByte(0xC2|($2<<3)); out_RelWord(&$4); }
				|	T_Z80_JP T_MODE_HL_IND
					{ out_AbsByte(0xE9); }
				|	T_Z80_JP T_MODE_HL
					{ out_AbsByte(0xE9); }
;

z80_jr			:	T_Z80_JR const_PCrel
					{ out_AbsByte(0x18); out_PCRelByte(&$2); }
				|	T_Z80_JR ccode comma const_PCrel
					{ out_AbsByte(0x20|($2<<3)); out_PCRelByte(&$4); }
;

z80_ldi			:	T_Z80_LDI T_MODE_HL_IND comma T_MODE_A
					{ out_AbsByte(0x02|(2<<4)); }
				|	T_Z80_LDI T_MODE_A comma T_MODE_HL
					{ out_AbsByte(0x0A|(2<<4)); }
				|	T_Z80_LDI T_MODE_A comma T_MODE_HL_IND
					{ out_AbsByte(0x0A|(2<<4)); }
;

z80_ldd			:	T_Z80_LDD T_MODE_HL_IND comma T_MODE_A
					{ out_AbsByte(0x02|(3<<4)); }
				|	T_Z80_LDD T_MODE_A comma T_MODE_HL
					{ out_AbsByte(0x0A|(3<<4)); }
				|	T_Z80_LDD T_MODE_A comma T_MODE_HL_IND
					{ out_AbsByte(0x0A|(3<<4)); }
;

z80_ldio		:	T_Z80_LDIO T_MODE_A comma op_mem_ind
					{
						rpn_CheckHRAM(&$4,&$4);

						if( (!rpn_isReloc(&$4))
						&&	($4.nVal<0 || ($4.nVal>0xFF && $4.nVal<0xFF00) || $4.nVal>0xFFFF) )
						{
							yyerror("Source address $%x not in HRAM ($FF00 to $FFFE)", $4.nVal);
						}

						out_AbsByte(0xF0);
						$4.nVal&=0xFF;
						out_RelByte(&$4);
					}
				|	T_Z80_LDIO op_mem_ind comma T_MODE_A
					{
						rpn_CheckHRAM(&$2,&$2);

						if( (!rpn_isReloc(&$2))
						&&	($2.nVal<0 || ($2.nVal>0xFF && $2.nVal<0xFF00) || $2.nVal>0xFFFF) )
						{
							yyerror("Destination address $%x not in HRAM ($FF00 to $FFFE)", $2.nVal);
						}

						out_AbsByte(0xE0);
						$2.nVal&=0xFF;
						out_RelByte(&$2);
					}
;

z80_ld			:	z80_ld_mem
				|	z80_ld_cind
				|	z80_ld_rr
				|	z80_ld_ss
				|	z80_ld_hl
				|	z80_ld_sp
				|	z80_ld_r
				|	z80_ld_a
;

z80_ld_hl		:	T_Z80_LD T_MODE_HL comma '[' T_MODE_SP const_8bit ']'
					{ out_AbsByte(0xF8); out_RelByte(&$6); }
				|	T_Z80_LD T_MODE_HL comma T_MODE_SP const_8bit
					{ out_AbsByte(0xF8); out_RelByte(&$5); }
				|	T_Z80_LD T_MODE_HL comma const_16bit
					{ out_AbsByte(0x01|(REG_HL<<4)); out_RelWord(&$4); }
;
z80_ld_sp		:	T_Z80_LD T_MODE_SP comma T_MODE_HL
					{ out_AbsByte(0xF9); }
				|	T_Z80_LD T_MODE_SP comma const_16bit
					{ out_AbsByte(0x01|(REG_SP<<4)); out_RelWord(&$4); }
;

z80_ld_mem		:	T_Z80_LD op_mem_ind comma T_MODE_SP
					{ out_AbsByte(0x08); out_RelWord(&$2); }
				|	T_Z80_LD op_mem_ind comma T_MODE_A
					{
						if( (!rpn_isReloc(&$2)) && $2.nVal>=0xFF00)
						{
							out_AbsByte(0xE0);
							out_AbsByte($2.nVal&0xFF);
						}
						else
						{
							out_AbsByte(0xEA);
							out_RelWord(&$2);
						}
					}
;

z80_ld_cind		:	T_Z80_LD T_MODE_C_IND comma T_MODE_A
					{ out_AbsByte(0xE2); }
;

z80_ld_rr		:	T_Z80_LD reg_rr comma T_MODE_A
					{ out_AbsByte(0x02|($2<<4)); }
;

z80_ld_r		:	T_Z80_LD reg_r comma const_8bit
					{ out_AbsByte(0x06|($2<<3)); out_RelByte(&$4); }
				|	T_Z80_LD reg_r comma reg_r
					{
						if( ($2==REG_HL_IND) && ($4==REG_HL_IND) )
						{
							yyerror("LD [HL],[HL] not a valid instruction");
						}
						else
							out_AbsByte(0x40|($2<<3)|$4);
					}
;

z80_ld_a		:	T_Z80_LD reg_r comma T_MODE_C_IND
					{
						if( $2==REG_A )
							out_AbsByte(0xF2);
						else
						{
							yyerror("Destination operand must be A");
						}
					}
				|	T_Z80_LD reg_r comma reg_rr
					{
						if( $2==REG_A )
							out_AbsByte(0x0A|($4<<4));
						else
						{
							yyerror("Destination operand must be A");
						}
					}
				|	T_Z80_LD reg_r comma op_mem_ind
					{
						if( $2==REG_A )
						{
							if( (!rpn_isReloc(&$4)) && $4.nVal>=0xFF00 )
							{
								out_AbsByte(0xF0);
								out_AbsByte($4.nVal&0xFF);
							}
							else
							{
								out_AbsByte(0xFA);
								out_RelWord(&$4);
							}
						}
						else
						{
							yyerror("Destination operand must be A");
						}
					}
;

z80_ld_ss		:	T_Z80_LD reg_ss comma const_16bit
					{ out_AbsByte(0x01|($2<<4)); out_RelWord(&$4); }
;

z80_nop			:	T_Z80_NOP
					{ out_AbsByte(0x00); }
;

z80_or			:	T_Z80_OR op_a_n
					{ out_AbsByte(0xF6); out_RelByte(&$2); }
				|	T_Z80_OR op_a_r
					{ out_AbsByte(0xB0|$2); }
;

z80_pop			:	T_Z80_POP reg_tt
					{ out_AbsByte(0xC1|($2<<4)); }
;

z80_push		:	T_Z80_PUSH reg_tt
					{ out_AbsByte(0xC5|($2<<4)); }
;

z80_res			:	T_Z80_RES const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x80|($2<<3)|$4); }
;

z80_ret			:	T_Z80_RET
					{ out_AbsByte(0xC9); }
				|	T_Z80_RET ccode
					{ out_AbsByte(0xC0|($2<<3)); }
;

z80_reti		:	T_Z80_RETI
					{ out_AbsByte(0xD9); }
;

z80_rl			:	T_Z80_RL reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x10|$2); }
;

z80_rla			:	T_Z80_RLA
					{ out_AbsByte(0x17); }
;

z80_rlc			:	T_Z80_RLC reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x00|$2); }
;

z80_rlca 		:	T_Z80_RLCA
					{ out_AbsByte(0x07); }
;

z80_rr			:	T_Z80_RR reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x18|$2); }
;

z80_rra			:	T_Z80_RRA
					{ out_AbsByte(0x1F); }
;

z80_rrc			:	T_Z80_RRC reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x08|$2); }
;

z80_rrca 		:	T_Z80_RRCA
					{ out_AbsByte(0x0F); }
;

z80_rst 		:	T_Z80_RST const_8bit
					{
						if( rpn_isReloc(&$2) )
						{
							yyerror("Address for RST must be absolute");
						}
						else if( ($2.nVal&0x38)!=$2.nVal )
						{
							yyerror("Invalid address $%x for RST", $2.nVal);
						}
						else
							out_AbsByte(0xC7|$2.nVal);
					}
;

z80_sbc			:	T_Z80_SBC op_a_n	{ out_AbsByte(0xDE); out_RelByte(&$2); }
				|	T_Z80_SBC op_a_r	{ out_AbsByte(0x98|$2); }
;

z80_scf			:	T_Z80_SCF
					{ out_AbsByte(0x37); }
;

z80_set			:	T_POP_SET const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0xC0|($2<<3)|$4); }
;

z80_sla			:	T_Z80_SLA reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x20|$2); }
;

z80_sra			:	T_Z80_SRA reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x28|$2); }
;

z80_srl			:	T_Z80_SRL reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x38|$2); }
;

z80_stop		:	T_Z80_STOP
					{ out_AbsByte(0x10); out_AbsByte(0x00); }
;

z80_sub			:	T_Z80_SUB op_a_n	{ out_AbsByte(0xD6); out_RelByte(&$2); }
				|	T_Z80_SUB op_a_r	{ out_AbsByte(0x90|$2); }
;

z80_swap		:	T_Z80_SWAP reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x30|$2); }
;

z80_xor			:	T_Z80_XOR op_a_n	{ out_AbsByte(0xEE); out_RelByte(&$2); }
				|	T_Z80_XOR op_a_r	{ out_AbsByte(0xA8|$2); }
;

op_mem_ind		:	'[' const_16bit ']'	{ $$ = $2; }
;

op_hl_ss 		:	reg_ss					{ $$ = $1; }
				|	T_MODE_HL comma reg_ss	{ $$ = $3; }
;

op_a_r			:	reg_r				{ $$ = $1; }
				|	T_MODE_A comma reg_r	{ $$ = $3; }
;

op_a_n			:	const_8bit				{ $$ = $1; }
				|	T_MODE_A comma const_8bit	{ $$ = $3; }
;

comma			:	','
;

ccode			:	T_CC_NZ		{ $$ = CC_NZ; }
				|	T_CC_Z		{ $$ = CC_Z; }
				|	T_CC_NC		{ $$ = CC_NC; }
				|	T_MODE_C	{ $$ = CC_C; }
;

reg_r			:	T_MODE_B		{ $$ = REG_B; }
				|	T_MODE_C		{ $$ = REG_C; }
				|	T_MODE_D		{ $$ = REG_D; }
				|	T_MODE_E		{ $$ = REG_E; }
				|	T_MODE_H		{ $$ = REG_H; }
				|	T_MODE_L		{ $$ = REG_L; }
				|	T_MODE_HL_IND	{ $$ = REG_HL_IND; }
				|	T_MODE_A		{ $$ = REG_A; }
;

reg_tt			:	T_MODE_BC		{ $$ = REG_BC; }
				|	T_MODE_DE		{ $$ = REG_DE; }
				|	T_MODE_HL		{ $$ = REG_HL; }
				|	T_MODE_AF		{ $$ = REG_AF; }
;

reg_ss			:	T_MODE_BC		{ $$ = REG_BC; }
				|	T_MODE_DE		{ $$ = REG_DE; }
				|	T_MODE_HL		{ $$ = REG_HL; }
				|	T_MODE_SP		{ $$ = REG_SP; }
;

reg_rr			:	T_MODE_BC_IND		{ $$ = REG_BC_IND; }
				|	T_MODE_DE_IND		{ $$ = REG_DE_IND; }
				|	T_MODE_HL_INDINC	{ $$ = REG_HL_INDINC; }
				|	T_MODE_HL_INDDEC	{ $$ = REG_HL_INDDEC; }
;

%%
