/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

%{
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/mymath.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/util.h"
#include "asm/warning.h"

#include "extern/utf8decoder.h"

#include "linkdefs.h"

uint32_t nListCountEmpty;
char *tzNewMacro;
uint32_t ulNewMacroSize;
int32_t nPCOffset;

size_t symvaluetostring(char *dest, size_t maxLength, char *symName,
			const char *mode)
{
	size_t length;
	struct sSymbol *sym = sym_FindSymbol(symName);

	if (sym && sym->type == SYM_EQUS) {
		char const *src = sym_GetStringValue(sym);
		size_t i;

		if (mode)
			yyerror("Print types are only allowed for numbers");

		for (i = 0; src[i] != 0; i++) {
			if (i >= maxLength)
				fatalerror("Symbol value too long to fit buffer");

			dest[i] = src[i];
		}

		length = i;

	} else {
		uint32_t value = sym_GetConstantValue(symName);
		int32_t fullLength;

		/* Special cheat for binary */
		if (mode && !mode[0]) {
			char binary[33]; /* 32 bits + 1 terminator */
			char *write_ptr = binary + 32;
			fullLength = 0;
			binary[32] = 0;
			do {
				*(--write_ptr) = (value & 1) + '0';
				value >>= 1;
				fullLength++;
			} while(value);
			strncpy(dest, write_ptr, maxLength + 1);
		} else {
			fullLength = snprintf(dest, maxLength + 1,
							  mode ? mode : "$%X",
						      value);
		}

		if (fullLength < 0) {
			fatalerror("snprintf encoding error");
		} else {
			length = (size_t)fullLength;
			if (length > maxLength)
				fatalerror("Symbol value too long to fit buffer");
		}
	}

	return length;
}

static uint32_t str2int2(char *s, int32_t length)
{
	int32_t i;
	uint32_t r = 0;

	i = length < 4 ? 0 : length - 4;
	while (i < length) {
		r <<= 8;
		r |= (uint8_t)s[i];
		i++;
	}

	return r;
}

static uint32_t isWhiteSpace(char s)
{
	return (s == ' ') || (s == '\t') || (s == '\0') || (s == '\n');
}

static uint32_t isRept(char *s)
{
	return (strncasecmp(s, "REPT", 4) == 0)
		&& isWhiteSpace(*(s - 1)) && isWhiteSpace(s[4]);
}

static uint32_t isEndr(char *s)
{
	return (strncasecmp(s, "ENDR", 4) == 0)
		&& isWhiteSpace(*(s - 1)) && isWhiteSpace(s[4]);
}

static void copyrept(void)
{
	int32_t level = 1, len, instring = 0;
	char *src = pCurrentBuffer->pBuffer;
	char *bufferEnd = pCurrentBuffer->pBufferStart
			+ pCurrentBuffer->nBufferSize;

	while (src < bufferEnd && level) {
		if (instring == 0) {
			if (isRept(src)) {
				level++;
				src += 4;
			} else if (isEndr(src)) {
				level--;
				src += 4;
			} else {
				if (*src == '\"')
					instring = 1;
				src++;
			}
		} else {
			if (*src == '\\') {
				src += 2;
			} else if (*src == '\"') {
				src++;
				instring = 0;
			} else {
				src++;
			}
		}
	}

	if (level != 0)
		fatalerror("Unterminated REPT block");

	len = src - pCurrentBuffer->pBuffer - 4;

	src = pCurrentBuffer->pBuffer;
	ulNewMacroSize = len;

	tzNewMacro = malloc(ulNewMacroSize + 1);

	if (tzNewMacro == NULL)
		fatalerror("Not enough memory for REPT block.");

	uint32_t i;

	tzNewMacro[ulNewMacroSize] = 0;
	for (i = 0; i < ulNewMacroSize; i++) {
		tzNewMacro[i] = src[i];
		if (src[i] == '\n')
			nLineNo++;
	}

	yyskipbytes(ulNewMacroSize + 4);

}

static uint32_t isMacro(char *s)
{
	return (strncasecmp(s, "MACRO", 4) == 0)
		&& isWhiteSpace(*(s - 1)) && isWhiteSpace(s[5]);
}

static uint32_t isEndm(char *s)
{
	return (strncasecmp(s, "ENDM", 4) == 0)
		&& isWhiteSpace(*(s - 1)) && isWhiteSpace(s[4]);
}

static void copymacro(void)
{
	int32_t level = 1, len, instring = 0;
	char *src = pCurrentBuffer->pBuffer;
	char *bufferEnd = pCurrentBuffer->pBufferStart
			+ pCurrentBuffer->nBufferSize;

	while (src < bufferEnd && level) {
		if (instring == 0) {
			if (isMacro(src)) {
				level++;
				src += 4;
			} else if (isEndm(src)) {
				level--;
				src += 4;
			} else {
				if(*src == '\"')
					instring = 1;
				src++;
			}
		} else {
			if (*src == '\\') {
				src += 2;
			} else if (*src == '\"') {
				src++;
				instring = 0;
			} else {
				src++;
			}
		}
	}

	if (level != 0)
		fatalerror("Unterminated MACRO definition.");

	len = src - pCurrentBuffer->pBuffer - 4;

	src = pCurrentBuffer->pBuffer;
	ulNewMacroSize = len;

	tzNewMacro = (char *)malloc(ulNewMacroSize + 1);
	if (tzNewMacro == NULL)
		fatalerror("Not enough memory for MACRO definition.");

	uint32_t i;

	tzNewMacro[ulNewMacroSize] = 0;
	for (i = 0; i < ulNewMacroSize; i++) {
		tzNewMacro[i] = src[i];
		if (src[i] == '\n')
			nLineNo++;
	}

	yyskipbytes(ulNewMacroSize + 4);
}

static bool endsIf(char c)
{
	return isWhiteSpace(c) || c == '(' || c == '{';
}

static uint32_t isIf(char *s)
{
	return (strncasecmp(s, "IF", 2) == 0)
		&& isWhiteSpace(s[-1]) && endsIf(s[2]);
}

static uint32_t isElif(char *s)
{
	return (strncasecmp(s, "ELIF", 4) == 0)
		&& isWhiteSpace(s[-1]) && endsIf(s[4]);
}

static uint32_t isElse(char *s)
{
	return (strncasecmp(s, "ELSE", 4) == 0)
		&& isWhiteSpace(s[-1]) && isWhiteSpace(s[4]);
}

static uint32_t isEndc(char *s)
{
	return (strncasecmp(s, "ENDC", 4) == 0)
		&& isWhiteSpace(s[-1]) && isWhiteSpace(s[4]);
}

static void if_skip_to_else(void)
{
	int32_t level = 1;
	bool inString = false;
	char *src = pCurrentBuffer->pBuffer;

	while (*src && level) {
		if (*src == '\n')
			nLineNo++;

		if (!inString) {
			if (isIf(src)) {
				level++;
				src += 2;

			} else if (level == 1 && isElif(src)) {
				level--;
				skipElif = false;

			} else if (level == 1 && isElse(src)) {
				level--;
				src += 4;

			} else if (isEndc(src)) {
				level--;
				if (level != 0)
					src += 4;

			} else {
				if (*src == '\"')
					inString = true;
				src++;
			}
		} else {
			if (*src == '\"') {
				inString = false;
			} else if (*src == '\\') {
				/* Escaped quotes don't end the string */
				if (*++src != '\"')
					src--;
			}
			src++;
		}
	}

	if (level != 0)
		fatalerror("Unterminated IF construct");

	int32_t len = src - pCurrentBuffer->pBuffer;

	yyskipbytes(len);
	yyunput('\n');
	nLineNo--;
}

static void if_skip_to_endc(void)
{
	int32_t level = 1;
	bool inString = false;
	char *src = pCurrentBuffer->pBuffer;

	while (*src && level) {
		if (*src == '\n')
			nLineNo++;

		if (!inString) {
			if (isIf(src)) {
				level++;
				src += 2;
			} else if (isEndc(src)) {
				level--;
				if (level != 0)
					src += 4;
			} else {
				if (*src == '\"')
					inString = true;
				src++;
			}
		} else {
			if (*src == '\"') {
				inString = false;
			} else if (*src == '\\') {
				/* Escaped quotes don't end the string */
				if (*++src != '\"')
					src--;
			}
			src++;
		}
	}

	if (level != 0)
		fatalerror("Unterminated IF construct");

	int32_t len = src - pCurrentBuffer->pBuffer;

	yyskipbytes(len);
	yyunput('\n');
	nLineNo--;
}

static void startUnion(void)
{
	if (!pCurrentSection)
		fatalerror("UNIONs must be inside a SECTION");

	uint32_t unionIndex = nUnionDepth;

	nUnionDepth++;
	if (nUnionDepth > MAXUNIONS)
		fatalerror("Too many nested UNIONs");

	unionStart[unionIndex] = nPC;
	unionSize[unionIndex] = 0;
}

static void updateUnion(void)
{
	uint32_t unionIndex = nUnionDepth - 1;
	uint32_t size = nPC - unionStart[unionIndex];

	if (size > unionSize[unionIndex])
		unionSize[unionIndex] = size;

	nPC = unionStart[unionIndex];
	pCurrentSection->nPC = unionStart[unionIndex];
}

static size_t strlenUTF8(const char *s)
{
	size_t len = 0;
	uint32_t state = 0;
	uint32_t codep = 0;

	while (*s) {
		switch (decode(&state, &codep, (uint8_t)*s)) {
		case 1:
			fatalerror("STRLEN: Invalid UTF-8 character");
			break;
		case 0:
			len++;
			break;
		}
		s++;
	}

	/* Check for partial code point. */
	if (state != 0)
		fatalerror("STRLEN: Invalid UTF-8 character");

	return len;
}

static void strsubUTF8(char *dest, const char *src, uint32_t pos, uint32_t len)
{
	size_t srcIndex = 0;
	size_t destIndex = 0;
	uint32_t state = 0;
	uint32_t codep = 0;
	uint32_t curPos = 1;
	uint32_t curLen = 0;

	if (pos < 1) {
		warning(WARNING_BUILTIN_ARG, "STRSUB: Position starts at 1");
		pos = 1;
	}

	/* Advance to starting position in source string. */
	while (src[srcIndex] && curPos < pos) {
		switch (decode(&state, &codep, (uint8_t)src[srcIndex])) {
		case 1:
			fatalerror("STRSUB: Invalid UTF-8 character");
			break;
		case 0:
			curPos++;
			break;
		}
		srcIndex++;
	}

	if (!src[srcIndex])
		warning(WARNING_BUILTIN_ARG, "STRSUB: Position %lu is past the end of the string",
			(unsigned long)pos);

	/* Copy from source to destination. */
	while (src[srcIndex] && destIndex < MAXSTRLEN && curLen < len) {
		switch (decode(&state, &codep, (uint8_t)src[srcIndex])) {
		case 1:
			fatalerror("STRSUB: Invalid UTF-8 character");
			break;
		case 0:
			curLen++;
			break;
		}
		dest[destIndex++] = src[srcIndex++];
	}

	if (curLen < len)
		warning(WARNING_BUILTIN_ARG, "STRSUB: Length too big: %lu", (unsigned long)len);

	/* Check for partial code point. */
	if (state != 0)
		fatalerror("STRSUB: Invalid UTF-8 character");

	dest[destIndex] = 0;
}

%}

%union
{
	char tzSym[MAXSYMLEN + 1];
	char tzString[MAXSTRLEN + 1];
	struct Expression sVal;
	int32_t nConstValue;
	struct SectionSpec sectSpec;
}

%type	<sVal>		relocexpr
%type	<sVal>		relocexpr_no_str
%type	<nConstValue>	const
%type	<nConstValue>	uconst
%type	<nConstValue>	const_3bit
%type	<sVal>		reloc_8bit
%type	<sVal>		reloc_8bit_no_str
%type	<sVal>		reloc_16bit
%type	<nConstValue>	sectiontype

%type	<tzString>	string

%type	<nConstValue>	sectorg
%type	<sectSpec>	sectattrs

%token	<nConstValue>	T_NUMBER
%token	<tzString>	T_STRING

%left	<nConstValue>	T_OP_LOGICNOT
%left	<nConstValue>	T_OP_LOGICOR T_OP_LOGICAND
%left	<nConstValue>	T_OP_LOGICGT T_OP_LOGICLT T_OP_LOGICGE T_OP_LOGICLE T_OP_LOGICNE T_OP_LOGICEQU
%left	<nConstValue>	T_OP_ADD T_OP_SUB
%left	<nConstValue>	T_OP_OR T_OP_XOR T_OP_AND
%left	<nConstValue>	T_OP_SHL T_OP_SHR
%left	<nConstValue>	T_OP_MUL T_OP_DIV T_OP_MOD
%left	<nConstValue>	T_OP_NOT
%left	<nConstValue>	T_OP_DEF
%left	<nConstValue>	T_OP_BANK T_OP_ALIGN
%left	<nConstValue>	T_OP_SIN
%left	<nConstValue>	T_OP_COS
%left	<nConstValue>	T_OP_TAN
%left	<nConstValue>	T_OP_ASIN
%left	<nConstValue>	T_OP_ACOS
%left	<nConstValue>	T_OP_ATAN
%left	<nConstValue>	T_OP_ATAN2
%left	<nConstValue>	T_OP_FDIV
%left	<nConstValue>	T_OP_FMUL
%left	<nConstValue>	T_OP_ROUND
%left	<nConstValue>	T_OP_CEIL
%left	<nConstValue>	T_OP_FLOOR

%token	<nConstValue>	T_OP_HIGH T_OP_LOW

%left	<nConstValue>	T_OP_STRCMP
%left	<nConstValue>	T_OP_STRIN
%left	<nConstValue>	T_OP_STRSUB
%left	<nConstValue>	T_OP_STRLEN
%left	<nConstValue>	T_OP_STRCAT
%left	<nConstValue>	T_OP_STRUPR
%left	<nConstValue>	T_OP_STRLWR

%left	NEG /* negation -- unary minus */

%token	<tzSym> T_LABEL
%token	<tzSym> T_ID
%token	<tzSym> T_POP_EQU
%token	<tzSym> T_POP_SET
%token	<tzSym> T_POP_EQUAL
%token	<tzSym> T_POP_EQUS

%token	T_POP_INCLUDE T_POP_PRINTF T_POP_PRINTT T_POP_PRINTV T_POP_PRINTI
%token	T_POP_IF T_POP_ELIF T_POP_ELSE T_POP_ENDC
%token	T_POP_IMPORT T_POP_EXPORT T_POP_GLOBAL
%token	T_POP_DB T_POP_DS T_POP_DW T_POP_DL
%token	T_POP_SECTION
%token	T_POP_RB
%token	T_POP_RW
%token	T_POP_RL
%token	T_POP_MACRO
%token	T_POP_ENDM
%token	T_POP_RSRESET T_POP_RSSET
%token	T_POP_UNION T_POP_NEXTU T_POP_ENDU
%token	T_POP_INCBIN T_POP_REPT
%token	T_POP_CHARMAP
%token	T_POP_NEWCHARMAP
%token	T_POP_SETCHARMAP
%token	T_POP_PUSHC
%token	T_POP_POPC
%token	T_POP_SHIFT
%token	T_POP_ENDR
%token	T_POP_LOAD T_POP_ENDL
%token	T_POP_FAIL
%token	T_POP_WARN
%token	T_POP_PURGE
%token	T_POP_POPS
%token	T_POP_PUSHS
%token	T_POP_POPO
%token	T_POP_PUSHO
%token	T_POP_OPT
%token	T_SECT_WRAM0 T_SECT_VRAM T_SECT_ROMX T_SECT_ROM0 T_SECT_HRAM
%token	T_SECT_WRAMX T_SECT_SRAM T_SECT_OAM
%token	T_SECT_HOME T_SECT_DATA T_SECT_CODE T_SECT_BSS

%token	T_Z80_ADC T_Z80_ADD T_Z80_AND
%token	T_Z80_BIT
%token	T_Z80_CALL T_Z80_CCF T_Z80_CP T_Z80_CPL
%token	T_Z80_DAA T_Z80_DEC T_Z80_DI
%token	T_Z80_EI
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

%token	T_TOKEN_A T_TOKEN_B T_TOKEN_C T_TOKEN_D T_TOKEN_E T_TOKEN_H T_TOKEN_L
%token	T_MODE_AF
%token	T_MODE_BC T_MODE_BC_IND
%token	T_MODE_DE T_MODE_DE_IND
%token	T_MODE_SP T_MODE_SP_IND
%token	T_MODE_C_IND
%token	T_MODE_HL T_MODE_HL_IND T_MODE_HL_INDDEC T_MODE_HL_INDINC
%token	T_CC_NZ T_CC_Z T_CC_NC

%type	<nConstValue>	reg_r
%type	<nConstValue>	reg_ss
%type	<nConstValue>	reg_rr
%type	<nConstValue>	reg_tt
%type	<nConstValue>	ccode
%type	<sVal>		op_a_n
%type	<nConstValue>	op_a_r
%type	<nConstValue>	op_hl_ss
%type	<sVal>		op_mem_ind
%start asmfile

%%

asmfile		: lines;

/* Note: The lexer adds '\n' at the end of the input */
lines		: /* empty */
		| lines {
				nListCountEmpty = 0;
				nPCOffset = 1;
			} line '\n' {
				nLineNo++;
				nTotalLines++;
			}
;

line		: label
		| label cpu_command
		| label macro
		| label simple_pseudoop
		| pseudoop
;

label		: /* empty */
		| T_LABEL
		{
			if ($1[0] == '.')
				sym_AddLocalReloc($1);
			else
				sym_AddReloc($1);
		}
		| T_LABEL ':'
		{
			if ($1[0] == '.')
				sym_AddLocalReloc($1);
			else
				sym_AddReloc($1);
		}
		| T_LABEL ':' ':'
		{
			if ($1[0] == '.')
				sym_AddLocalReloc($1);
			else
				sym_AddReloc($1);
			sym_Export($1);
		}
;

macro		: T_ID {
				yy_set_state(LEX_STATE_MACROARGS);
			} macroargs {
				yy_set_state(LEX_STATE_NORMAL);
				if (!fstk_RunMacro($1))
					fatalerror("Macro '%s' not defined", $1);
			}
;

macroargs	: /* empty */
		| macroarg
		| macroarg comma macroargs
;

macroarg	: T_STRING	{ sym_AddNewMacroArg($1); }
;

pseudoop	: equ
		| set
		| rb
		| rw
		| rl
		| equs
		| macrodef
;

simple_pseudoop : include
		| printf
		| printt
		| printv
		| printi
		| if
		| elif
		| else
		| endc
		| import
		| export
		| global
		| { nPCOffset = 0; } db
		| { nPCOffset = 0; } dw
		| { nPCOffset = 0; } dl
		| ds
		| section
		| rsreset
		| rsset
		| union
		| nextu
		| endu
		| incbin
		| charmap
		| newcharmap
		| setcharmap
		| pushc
		| popc
		| load
		| rept
		| shift
		| fail
		| warn
		| purge
		| pops
		| pushs
		| popo
		| pusho
		| opt
;

opt		: T_POP_OPT {
				yy_set_state(LEX_STATE_MACROARGS);
			} opt_list {
				yy_set_state(LEX_STATE_NORMAL);
			}
;

opt_list	: opt_list_entry
		| opt_list_entry comma opt_list
;

opt_list_entry	: T_STRING		{ opt_Parse($1); }
;

popo		: T_POP_POPO		{ opt_Pop(); }
;

pusho		: T_POP_PUSHO		{ opt_Push(); }
;

pops		: T_POP_POPS		{ out_PopSection(); }
;

pushs		: T_POP_PUSHS		{ out_PushSection(); }
;

fail		: T_POP_FAIL string	{ fatalerror("%s", $2); }
;

warn		: T_POP_WARN string	{ warning(WARNING_USER, "%s", $2); }
;

shift		: T_POP_SHIFT		{ sym_ShiftCurrentMacroArgs(); }
		| T_POP_SHIFT uconst
		{
			int32_t i = $2;
			while (i--)
				sym_ShiftCurrentMacroArgs();
		}
;

load		: T_POP_LOAD string comma sectiontype sectorg sectattrs
		{
			out_SetLoadSection($2, $4, $5, &$6);
		}
		| T_POP_ENDL
		{
			out_EndLoadSection();
		}
;

rept		: T_POP_REPT uconst
		{
			uint32_t nDefinitionLineNo = nLineNo;
			copyrept();
			fstk_RunRept($2, nDefinitionLineNo);
		}
;

macrodef	: T_LABEL ':' T_POP_MACRO
		{
			int32_t nDefinitionLineNo = nLineNo;
			copymacro();
			sym_AddMacro($1, nDefinitionLineNo);
		}
;

equs		: T_LABEL T_POP_EQUS string
		{
			sym_AddString($1, $3);
		}
;

rsset		: T_POP_RSSET uconst
		{
			sym_AddSet("_RS", $2);
		}
;

rsreset		: T_POP_RSRESET
		{
			sym_AddSet("_RS", 0);
		}
;

rl		: T_LABEL T_POP_RL uconst
		{
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + 4 * $3);
		}
;

rw		: T_LABEL T_POP_RW uconst
		{
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + 2 * $3);
		}
;

rb		: T_LABEL T_POP_RB uconst
		{
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + $3);
		}
;

union		: T_POP_UNION
		{
			startUnion();
		}
;

nextu		: T_POP_NEXTU
		{
			if (nUnionDepth <= 0)
				fatalerror("Found NEXTU outside of a UNION construct");

			updateUnion();
		}
;

endu		: T_POP_ENDU
		{
			if (nUnionDepth <= 0)
				fatalerror("Found ENDU outside of a UNION construct");

			updateUnion();

			nUnionDepth--;
			nPC = unionStart[nUnionDepth] + unionSize[nUnionDepth];
			pCurrentSection->nPC = nPC;
		}
;

ds		: T_POP_DS uconst
		{
			out_Skip($2);
		}
;

db		: T_POP_DB constlist_8bit_entry comma constlist_8bit {
			if (nListCountEmpty > 0) {
				warning(WARNING_EMPTY_ENTRY, "Empty entry in list of 8-bit elements (treated as padding).");
			}
		}
		| T_POP_DB constlist_8bit_entry
;

dw		: T_POP_DW constlist_16bit_entry comma constlist_16bit {
			if (nListCountEmpty > 0) {
				warning(WARNING_EMPTY_ENTRY, "Empty entry in list of 16-bit elements (treated as padding).");
			}
		}
		| T_POP_DW constlist_16bit_entry
;

dl		: T_POP_DL constlist_32bit_entry comma constlist_32bit {
			if (nListCountEmpty > 0) {
				warning(WARNING_EMPTY_ENTRY, "Empty entry in list of 32-bit elements (treated as padding).");
			}
		}
		| T_POP_DL constlist_32bit_entry
;

purge		: T_POP_PURGE {
				oDontExpandStrings = true;
			} purge_list {
				oDontExpandStrings = false;
			}
;

purge_list	: purge_list_entry
		| purge_list_entry comma purge_list
;

purge_list_entry : T_ID
		{
			sym_Purge($1);
		}
;

import		: T_POP_IMPORT import_list
;

import_list	: import_list_entry
		| import_list_entry comma import_list
;

import_list_entry : T_ID
		{
			/*
			 * This is done automatically if the label isn't found
			 * in the list of defined symbols.
			 */
			warning(WARNING_OBSOLETE, "IMPORT is a deprecated keyword with no effect: %s", $1);
		}
;

export		: T_POP_EXPORT export_list
;

export_list	: export_list_entry
		| export_list_entry comma export_list
;

export_list_entry : T_ID
		{
			sym_Export($1);
		}
;

global		: T_POP_GLOBAL global_list
;

global_list	: global_list_entry
		| global_list_entry comma global_list
;

global_list_entry : T_ID
		{
			sym_Export($1);
		}
;

equ		: T_LABEL T_POP_EQU const
		{
			sym_AddEqu($1, $3);
		}
;

set		: T_LABEL T_POP_SET const
		{
			sym_AddSet($1, $3);
		}
		| T_LABEL T_POP_EQUAL const
		{
			sym_AddSet($1, $3);
		}
;

include		: T_POP_INCLUDE string
		{
			fstk_RunInclude($2);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
;

incbin		: T_POP_INCBIN string
		{
			out_BinaryFile($2);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
		| T_POP_INCBIN string comma uconst comma uconst
		{
			out_BinaryFileSlice($2, $4, $6);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
;

charmap		: T_POP_CHARMAP string comma const
		{
			if (($4 & 0xFF) != $4)
				warning(WARNING_TRUNCATION, "Expression must be 8-bit");

			if (charmap_Add($2, $4 & 0xFF) == -1)
				yyerror("Error parsing charmap. Either you've added too many (%i), or the input character length is too long (%i)' : %s\n", MAXCHARMAPS, CHARMAPLENGTH, strerror(errno));
		}
;

newcharmap	: T_POP_NEWCHARMAP T_ID
		{
			charmap_New($2, NULL);
		}
		| T_POP_NEWCHARMAP T_ID comma T_ID
		{
			charmap_New($2, $4);
		}
;

setcharmap	: T_POP_SETCHARMAP T_ID
		{
			charmap_Set($2);
		}
;

pushc		: T_POP_PUSHC	{ charmap_Push(); }
;

popc		: T_POP_POPC	{ charmap_Pop(); }
;

printt		: T_POP_PRINTT string
		{
			printf("%s", $2);
		}
;

printv		: T_POP_PRINTV const
		{
			printf("$%X", $2);
		}
;

printi		: T_POP_PRINTI const
		{
			printf("%d", $2);
		}
;

printf		: T_POP_PRINTF const
		{
			math_Print($2);
		}
;

if		: T_POP_IF const
		{
			nIFDepth++;
			if (!$2) {
				/*
				 * Continue parsing after ELSE, or at ELIF or
				 * ENDC keyword.
				 */
				if_skip_to_else();
			}
		}
;

elif		: T_POP_ELIF const
		{
			if (nIFDepth <= 0)
				fatalerror("Found ELIF outside an IF construct");

			if (skipElif) {
				/*
				 * Executed when ELIF is reached at the end of
				 * an IF or ELIF block for which the condition
				 * was true.
				 *
				 * Continue parsing at ENDC keyword
				 */
				if_skip_to_endc();
			} else {
				/*
				 * Executed when ELIF is skipped to because the
				 * condition of the previous IF or ELIF block
				 * was false.
				 */
				skipElif = true;

				if (!$2) {
					/*
					 * Continue parsing after ELSE, or at
					 * ELIF or ENDC keyword.
					 */
					if_skip_to_else();
				}
			}
		}
;

else		: T_POP_ELSE
		{
			if (nIFDepth <= 0)
				fatalerror("Found ELSE outside an IF construct");

			/* Continue parsing at ENDC keyword */
			if_skip_to_endc();
		}
;

endc		: T_POP_ENDC
		{
			if (nIFDepth <= 0)
				fatalerror("Found ENDC outside an IF construct");

			nIFDepth--;
		}
;

const_3bit	: const
		{
			int32_t value = $1;
			if ((value < 0) || (value > 7))
				yyerror("Immediate value must be 3-bit");
			else
				$$ = value & 0x7;
		}
;

constlist_8bit	: constlist_8bit_entry
		| constlist_8bit_entry comma constlist_8bit
;

constlist_8bit_entry : /* empty */
		{
			out_Skip(1);
			nListCountEmpty++;
		}
		| reloc_8bit_no_str
		{
			out_RelByte(&$1);
		}
		| string
		{
			char *s = $1;
			int32_t length = charmap_Convert(&s);

			out_AbsByteGroup((uint8_t*)s, length);
			free(s);
		}
;

constlist_16bit : constlist_16bit_entry
		| constlist_16bit_entry comma constlist_16bit
;

constlist_16bit_entry : /* empty */
		{
			out_Skip(2);
			nListCountEmpty++;
		}
		| reloc_16bit
		{
			out_RelWord(&$1);
		}
;

constlist_32bit : constlist_32bit_entry
		| constlist_32bit_entry comma constlist_32bit
;

constlist_32bit_entry : /* empty */
		{
			out_Skip(4);
			nListCountEmpty++;
		}
		| relocexpr
		{
			out_RelLong(&$1);
		}
;

reloc_8bit	: relocexpr
		{
			if( (rpn_isKnown(&$1)) && (($1.nVal < -128) || ($1.nVal > 255)) )
				warning(WARNING_TRUNCATION, "Expression must be 8-bit");
			$$ = $1;
		}
;

reloc_8bit_no_str	: relocexpr_no_str
		{
			if( (rpn_isKnown(&$1)) && (($1.nVal < -128) || ($1.nVal > 255)) )
				warning(WARNING_TRUNCATION, "Expression must be 8-bit");
			$$ = $1;
		}
;

reloc_16bit	: relocexpr
		{
			if ((rpn_isKnown(&$1)) && (($1.nVal < -32768) || ($1.nVal > 65535)))
				warning(WARNING_TRUNCATION, "Expression must be 16-bit");
			$$ = $1;
		}
;


relocexpr	: relocexpr_no_str
		| string
		{
			char *s = $1;
			int32_t length = charmap_Convert(&s);
			uint32_t r = str2int2(s, length);

			free(s);
			rpn_Number(&$$, r);
		}
;

relocexpr_no_str	: T_ID
		{
			rpn_Symbol(&$$, $1);
		}
		| T_NUMBER
		{
			rpn_Number(&$$, $1);
		}
		| T_OP_LOGICNOT relocexpr %prec NEG	{ rpn_LOGNOT(&$$, &$2); }
		| relocexpr T_OP_LOGICOR relocexpr	{ rpn_BinaryOp(RPN_LOGOR, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICAND relocexpr	{ rpn_BinaryOp(RPN_LOGAND, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICEQU relocexpr	{ rpn_BinaryOp(RPN_LOGEQ, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICGT relocexpr	{ rpn_BinaryOp(RPN_LOGGT, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICLT relocexpr	{ rpn_BinaryOp(RPN_LOGLT, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICGE relocexpr	{ rpn_BinaryOp(RPN_LOGGE, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICLE relocexpr	{ rpn_BinaryOp(RPN_LOGLE, &$$, &$1, &$3); }
		| relocexpr T_OP_LOGICNE relocexpr	{ rpn_BinaryOp(RPN_LOGNE, &$$, &$1, &$3); }
		| relocexpr T_OP_ADD relocexpr	{ rpn_BinaryOp(RPN_ADD, &$$, &$1, &$3); }
		| relocexpr T_OP_SUB relocexpr	{ rpn_BinaryOp(RPN_SUB, &$$, &$1, &$3); }
		| relocexpr T_OP_XOR relocexpr	{ rpn_BinaryOp(RPN_XOR, &$$, &$1, &$3); }
		| relocexpr T_OP_OR relocexpr		{ rpn_BinaryOp(RPN_OR, &$$, &$1, &$3); }
		| relocexpr T_OP_AND relocexpr	{ rpn_BinaryOp(RPN_AND, &$$, &$1, &$3); }
		| relocexpr T_OP_SHL relocexpr	{ rpn_BinaryOp(RPN_SHL, &$$, &$1, &$3); }
		| relocexpr T_OP_SHR relocexpr	{ rpn_BinaryOp(RPN_SHR, &$$, &$1, &$3); }
		| relocexpr T_OP_MUL relocexpr	{ rpn_BinaryOp(RPN_MUL, &$$, &$1, &$3); }
		| relocexpr T_OP_DIV relocexpr	{ rpn_BinaryOp(RPN_DIV, &$$, &$1, &$3); }
		| relocexpr T_OP_MOD relocexpr	{ rpn_BinaryOp(RPN_MOD, &$$, &$1, &$3); }
		| T_OP_ADD relocexpr %prec NEG		{ $$ = $2; }
		| T_OP_SUB relocexpr %prec NEG		{ rpn_UNNEG(&$$, &$2); }
		| T_OP_NOT relocexpr %prec NEG		{ rpn_UNNOT(&$$, &$2); }
		| T_OP_HIGH '(' relocexpr ')'		{ rpn_HIGH(&$$, &$3); }
		| T_OP_LOW '(' relocexpr ')'		{ rpn_LOW(&$$, &$3); }
		| T_OP_BANK '(' T_ID ')'
		{
			/* '@' is also a T_ID, it is handled here. */
			rpn_BankSymbol(&$$, $3);
		}
		| T_OP_BANK '(' string ')'
		{
			rpn_BankSection(&$$, $3);
		}
		| T_OP_DEF {
				oDontExpandStrings = true;
			} '(' T_ID ')'
		{
			struct sSymbol const *sym = sym_FindSymbol($4);
			if (sym && !(sym_IsDefined(sym) && sym->type != SYM_LABEL))
				yyerror("Label \"%s\" is not a valid argument to DEF",
					$4);
			rpn_Number(&$$, !!sym);
			oDontExpandStrings = false;
		}
		| T_OP_ROUND '(' const ')'
		{
			rpn_Number(&$$, math_Round($3));
		}
		| T_OP_CEIL '(' const ')'
		{
			rpn_Number(&$$, math_Ceil($3));
		}
		| T_OP_FLOOR '(' const ')'
		{
			rpn_Number(&$$, math_Floor($3));
		}
		| T_OP_FDIV '(' const comma const ')'
		{
			rpn_Number(&$$,
				   math_Div($3,
					    $5));
		}
		| T_OP_FMUL '(' const comma const ')'
		{
			rpn_Number(&$$,
				   math_Mul($3,
					    $5));
		}
		| T_OP_SIN '(' const ')'
		{
			rpn_Number(&$$, math_Sin($3));
		}
		| T_OP_COS '(' const ')'
		{
			rpn_Number(&$$, math_Cos($3));
		}
		| T_OP_TAN '(' const ')'
		{
			rpn_Number(&$$, math_Tan($3));
		}
		| T_OP_ASIN '(' const ')'
		{
			rpn_Number(&$$, math_ASin($3));
		}
		| T_OP_ACOS '(' const ')'
		{
			rpn_Number(&$$, math_ACos($3));
		}
		| T_OP_ATAN '(' const ')'
		{
			rpn_Number(&$$, math_ATan($3));
		}
		| T_OP_ATAN2 '(' const comma const ')'
		{
			rpn_Number(&$$,
				   math_ATan2($3,
					      $5));
		}
		| T_OP_STRCMP '(' string comma string ')'
		{
			rpn_Number(&$$, strcmp($3, $5));
		}
		| T_OP_STRIN '(' string comma string ')'
		{
			char *p = strstr($3, $5);

			if (p != NULL)
				rpn_Number(&$$, p - $3 + 1);
			else
				rpn_Number(&$$, 0);
		}
		| T_OP_STRLEN '(' string ')'		{ rpn_Number(&$$, strlenUTF8($3)); }
		| '(' relocexpr ')'			{ $$ = $2; }
;

uconst		: const
		{
			int32_t value = $1;
			if (value < 0)
				fatalerror("Constant mustn't be negative: %d", value);
			$$ = value;
		}
;

const		: relocexpr
		{
			if (!rpn_isKnown(&$1)) {
				yyerror("Expected constant expression: %s",
					$1.reason);
				$$ = 0;
			} else {
				$$ = $1.nVal;
			}
		}
;

string		: T_STRING
		{
			if (snprintf($$, MAXSTRLEN + 1, "%s", $1) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "String is too long '%s'", $1);
		}
		| T_OP_STRSUB '(' string comma uconst comma uconst ')'
		{
			strsubUTF8($$, $3, $5, $7);
		}
		| T_OP_STRCAT '(' string comma string ')'
		{
			if (snprintf($$, MAXSTRLEN + 1, "%s%s", $3, $5) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRCAT: String too long '%s%s'", $3, $5);
		}
		| T_OP_STRUPR '(' string ')'
		{
			if (snprintf($$, MAXSTRLEN + 1, "%s", $3) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRUPR: String too long '%s'", $3);

			upperstring($$);
		}
		| T_OP_STRLWR '(' string ')'
		{
			if (snprintf($$, MAXSTRLEN + 1, "%s", $3) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRUPR: String too long '%s'", $3);

			lowerstring($$);
		}
;

section		: T_POP_SECTION string comma sectiontype sectorg sectattrs
		{
			out_NewSection($2, $4, $5, &$6);
		}
;

sectiontype	: T_SECT_WRAM0	{ $$ = SECTTYPE_WRAM0; }
		| T_SECT_VRAM	{ $$ = SECTTYPE_VRAM; }
		| T_SECT_ROMX	{ $$ = SECTTYPE_ROMX; }
		| T_SECT_ROM0	{ $$ = SECTTYPE_ROM0; }
		| T_SECT_HRAM	{ $$ = SECTTYPE_HRAM; }
		| T_SECT_WRAMX	{ $$ = SECTTYPE_WRAMX; }
		| T_SECT_SRAM	{ $$ = SECTTYPE_SRAM; }
		| T_SECT_OAM	{ $$ = SECTTYPE_OAM; }
		| T_SECT_HOME
		{
			warning(WARNING_OBSOLETE, "HOME section name is deprecated, use ROM0 instead.");
			$$ = SECTTYPE_ROM0;
		}
		| T_SECT_DATA
		{
			warning(WARNING_OBSOLETE, "DATA section name is deprecated, use ROMX instead.");
			$$ = SECTTYPE_ROMX;
		}
		| T_SECT_CODE
		{
			warning(WARNING_OBSOLETE, "CODE section name is deprecated, use ROMX instead.");
			$$ = SECTTYPE_ROMX;
		}
		| T_SECT_BSS
		{
			warning(WARNING_OBSOLETE, "BSS section name is deprecated, use WRAM0 instead.");
			$$ = SECTTYPE_WRAM0;
		}
;

sectorg		: { $$ = -1; }
		| '[' uconst ']'
		{
			if ($2 < 0 || $2 >= 0x10000)
				yyerror("Address $%x is not 16-bit", $2);
			else
				$$ = $2;
		}
;

sectattrs	:
		{
			$$.alignment = 0;
			$$.bank = -1;
		}
		| sectattrs comma T_OP_ALIGN '[' uconst ']'
		{
			if ($5 < 0 || $5 > 16)
				yyerror("Alignment must be between 0 and 16 bits, not %u",
					$5);
			else
				$$.alignment = $5;
		}
		| sectattrs comma T_OP_BANK '[' uconst ']'
		{
			/* We cannot check the validity of this now */
			$$.bank = $5;
		}
;


cpu_command	: z80_adc
		| z80_add
		| z80_and
		| z80_bit
		| z80_call
		| z80_ccf
		| z80_cp
		| z80_cpl
		| z80_daa
		| z80_dec
		| z80_di
		| z80_ei
		| z80_halt
		| z80_inc
		| z80_jp
		| z80_jr
		| z80_ld
		| z80_ldd
		| z80_ldi
		| z80_ldio
		| z80_nop
		| z80_or
		| z80_pop
		| z80_push
		| z80_res
		| z80_ret
		| z80_reti
		| z80_rl
		| z80_rla
		| z80_rlc
		| z80_rlca
		| z80_rr
		| z80_rra
		| z80_rrc
		| z80_rrca
		| z80_rst
		| z80_sbc
		| z80_scf
		| z80_set
		| z80_sla
		| z80_sra
		| z80_srl
		| z80_stop
		| z80_sub
		| z80_swap
		| z80_xor
;

z80_adc		: T_Z80_ADC op_a_n
		{
			out_AbsByte(0xCE);
			out_RelByte(&$2);
		}
		| T_Z80_ADC op_a_r
		{
			out_AbsByte(0x88 | $2);
		}
;

z80_add		: T_Z80_ADD op_a_n
		{
			out_AbsByte(0xC6);
			out_RelByte(&$2);
		}
		| T_Z80_ADD op_a_r
		{
			out_AbsByte(0x80 | $2);
		}
		| T_Z80_ADD op_hl_ss
		{
			out_AbsByte(0x09 | ($2 << 4));
		}
		| T_Z80_ADD T_MODE_SP comma reloc_8bit
		{
			out_AbsByte(0xE8);
			out_RelByte(&$4);
		}

;

z80_and		: T_Z80_AND op_a_n
		{
			out_AbsByte(0xE6);
			out_RelByte(&$2);
		}
		| T_Z80_AND op_a_r
		{
			out_AbsByte(0xA0 | $2);
		}
;

z80_bit		: T_Z80_BIT const_3bit comma reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x40 | ($2 << 3) | $4);
		}
;

z80_call	: T_Z80_CALL reloc_16bit
		{
			out_AbsByte(0xCD);
			out_RelWord(&$2);
		}
		| T_Z80_CALL ccode comma reloc_16bit
		{
			out_AbsByte(0xC4 | ($2 << 3));
			out_RelWord(&$4);
		}
;

z80_ccf		: T_Z80_CCF
		{
			out_AbsByte(0x3F);
		}
;

z80_cp		: T_Z80_CP op_a_n
		{
			out_AbsByte(0xFE);
			out_RelByte(&$2);
		}
		| T_Z80_CP op_a_r
		{
			out_AbsByte(0xB8 | $2);
		}
;

z80_cpl		: T_Z80_CPL
		{
			out_AbsByte(0x2F);
		}
;

z80_daa		: T_Z80_DAA
		{
			out_AbsByte(0x27);
		}
;

z80_dec		: T_Z80_DEC reg_r
		{
			out_AbsByte(0x05 | ($2 << 3));
		}
		| T_Z80_DEC reg_ss
		{
			out_AbsByte(0x0B | ($2 << 4));
		}
;

z80_di		: T_Z80_DI
		{
			out_AbsByte(0xF3);
		}
;

z80_ei		: T_Z80_EI
		{
			out_AbsByte(0xFB);
		}
;

z80_halt	: T_Z80_HALT
		{
			out_AbsByte(0x76);
			if (CurrentOptions.haltnop)
				out_AbsByte(0x00);
		}
;

z80_inc		: T_Z80_INC reg_r
		{
			out_AbsByte(0x04 | ($2 << 3));
		}
		| T_Z80_INC reg_ss
		{
			out_AbsByte(0x03 | ($2 << 4));
		}
;

z80_jp		: T_Z80_JP reloc_16bit
		{
			out_AbsByte(0xC3);
			out_RelWord(&$2);
		}
		| T_Z80_JP ccode comma reloc_16bit
		{
			out_AbsByte(0xC2 | ($2 << 3));
			out_RelWord(&$4);
		}
		| T_Z80_JP T_MODE_HL_IND
		{
			out_AbsByte(0xE9);
			warning(WARNING_OBSOLETE, "'JP [HL]' is obsolete, use 'JP HL' instead.");
		}
		| T_Z80_JP T_MODE_HL
		{
			out_AbsByte(0xE9);
		}
;

z80_jr		: T_Z80_JR reloc_16bit
		{
			out_AbsByte(0x18);
			out_PCRelByte(&$2);
		}
		| T_Z80_JR ccode comma reloc_16bit
		{
			out_AbsByte(0x20 | ($2 << 3));
			out_PCRelByte(&$4);
		}
;

z80_ldi		: T_Z80_LDI T_MODE_HL_IND comma T_MODE_A
		{
			out_AbsByte(0x02 | (2 << 4));
		}
		| T_Z80_LDI T_MODE_A comma T_MODE_HL
		{
			out_AbsByte(0x0A | (2 << 4));
			warning(WARNING_OBSOLETE, "'LDI A,HL' is obsolete, use 'LDI A,[HL]' or 'LD A,[HL+] instead.");
		}
		| T_Z80_LDI T_MODE_A comma T_MODE_HL_IND
		{
			out_AbsByte(0x0A | (2 << 4));
		}
;

z80_ldd		: T_Z80_LDD T_MODE_HL_IND comma T_MODE_A
		{
			out_AbsByte(0x02 | (3 << 4));
		}
		| T_Z80_LDD T_MODE_A comma T_MODE_HL
		{
			out_AbsByte(0x0A | (3 << 4));
			warning(WARNING_OBSOLETE, "'LDD A,HL' is obsolete, use 'LDD A,[HL]' or 'LD A,[HL-] instead.");
		}
		| T_Z80_LDD T_MODE_A comma T_MODE_HL_IND
		{
			out_AbsByte(0x0A | (3 << 4));
		}
;

z80_ldio	: T_Z80_LDIO T_MODE_A comma op_mem_ind
		{
			rpn_CheckHRAM(&$4, &$4);

			if ((rpn_isKnown(&$4)) && ($4.nVal < 0 || ($4.nVal > 0xFF && $4.nVal < 0xFF00) || $4.nVal > 0xFFFF))
				yyerror("Source address $%x not in $FF00 to $FFFF", $4.nVal);

			out_AbsByte(0xF0);
			$4.nVal &= 0xFF;
			out_RelByte(&$4);
		}
		| T_Z80_LDIO op_mem_ind comma T_MODE_A
		{
			rpn_CheckHRAM(&$2, &$2);

			if ((rpn_isKnown(&$2)) && ($2.nVal < 0 || ($2.nVal > 0xFF && $2.nVal < 0xFF00) || $2.nVal > 0xFFFF))
				yyerror("Destination address $%x not in $FF00 to $FFFF", $2.nVal);

			out_AbsByte(0xE0);
			$2.nVal &= 0xFF;
			out_RelByte(&$2);
		}
		| T_Z80_LDIO T_MODE_A comma T_MODE_C_IND
		{
			out_AbsByte(0xF2);
		}
		| T_Z80_LDIO T_MODE_C_IND comma T_MODE_A
		{
			out_AbsByte(0xE2);
		}
;

z80_ld		: z80_ld_mem
		| z80_ld_cind
		| z80_ld_rr
		| z80_ld_ss
		| z80_ld_hl
		| z80_ld_sp
		| z80_ld_r
		| z80_ld_a
;

z80_ld_hl	: T_Z80_LD T_MODE_HL comma '[' T_MODE_SP reloc_8bit ']'
		{
			out_AbsByte(0xF8);
			out_RelByte(&$6);
			warning(WARNING_OBSOLETE, "'LD HL,[SP+e8]' is obsolete, use 'LD HL,SP+e8' instead.");
		}
		| T_Z80_LD T_MODE_HL comma T_MODE_SP reloc_8bit
		{
			out_AbsByte(0xF8);
			out_RelByte(&$5);
		}
		| T_Z80_LD T_MODE_HL comma reloc_16bit
		{
			out_AbsByte(0x01 | (REG_HL << 4));
			out_RelWord(&$4);
		}
;

z80_ld_sp	: T_Z80_LD T_MODE_SP comma T_MODE_HL
		{
			out_AbsByte(0xF9);
		}
		| T_Z80_LD T_MODE_SP comma reloc_16bit
		{
			out_AbsByte(0x01 | (REG_SP << 4));
			out_RelWord(&$4);
		}
;

z80_ld_mem	: T_Z80_LD op_mem_ind comma T_MODE_SP
		{
			out_AbsByte(0x08);
			out_RelWord(&$2);
		}
		| T_Z80_LD op_mem_ind comma T_MODE_A
		{
			if (CurrentOptions.optimizeloads &&
			    (rpn_isKnown(&$2)) && ($2.nVal >= 0xFF00)) {
				out_AbsByte(0xE0);
				out_AbsByte($2.nVal & 0xFF);
				rpn_Free(&$2);
			} else {
				out_AbsByte(0xEA);
				out_RelWord(&$2);
			}
		}
;

z80_ld_cind	: T_Z80_LD T_MODE_C_IND comma T_MODE_A
		{
			out_AbsByte(0xE2);
		}
;

z80_ld_rr	: T_Z80_LD reg_rr comma T_MODE_A
		{
			out_AbsByte(0x02 | ($2 << 4));
		}
;

z80_ld_r	: T_Z80_LD reg_r comma reloc_8bit
		{
			out_AbsByte(0x06 | ($2 << 3));
			out_RelByte(&$4);
		}
		| T_Z80_LD reg_r comma reg_r
		{
			if (($2 == REG_HL_IND) && ($4 == REG_HL_IND))
				yyerror("LD [HL],[HL] not a valid instruction");
			else
				out_AbsByte(0x40 | ($2 << 3) | $4);
		}
;

z80_ld_a	: T_Z80_LD reg_r comma T_MODE_C_IND
		{
			if ($2 == REG_A)
				out_AbsByte(0xF2);
			else
				yyerror("Destination operand must be A");
		}
		| T_Z80_LD reg_r comma reg_rr
		{
			if ($2 == REG_A)
				out_AbsByte(0x0A | ($4 << 4));
			else
				yyerror("Destination operand must be A");
		}
		| T_Z80_LD reg_r comma op_mem_ind
		{
			if ($2 == REG_A) {
				if (CurrentOptions.optimizeloads &&
				    (rpn_isKnown(&$4)) && ($4.nVal >= 0xFF00)) {
					out_AbsByte(0xF0);
					out_AbsByte($4.nVal & 0xFF);
					rpn_Free(&$4);
				} else {
					out_AbsByte(0xFA);
					out_RelWord(&$4);
				}
			} else {
				yyerror("Destination operand must be A");
				rpn_Free(&$4);
			}
		}
;

z80_ld_ss	: T_Z80_LD T_MODE_BC comma reloc_16bit
		{
			out_AbsByte(0x01 | (REG_BC << 4));
			out_RelWord(&$4);
		}
		| T_Z80_LD T_MODE_DE comma reloc_16bit
		{
			out_AbsByte(0x01 | (REG_DE << 4));
			out_RelWord(&$4);
		}
		/*
		 * HL is taken care of in z80_ld_hl
		 * SP is taken care of in z80_ld_sp
		 */
;

z80_nop		: T_Z80_NOP
		{
			out_AbsByte(0x00);
		}
;

z80_or		: T_Z80_OR op_a_n
		{
			out_AbsByte(0xF6);
			out_RelByte(&$2);
		}
		| T_Z80_OR op_a_r
		{
			out_AbsByte(0xB0 | $2);
		}
;

z80_pop		: T_Z80_POP reg_tt
		{
			out_AbsByte(0xC1 | ($2 << 4));
		}
;

z80_push	: T_Z80_PUSH reg_tt
		{
			out_AbsByte(0xC5 | ($2 << 4));
		}
;

z80_res		: T_Z80_RES const_3bit comma reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x80 | ($2 << 3) | $4);
		}
;

z80_ret		: T_Z80_RET
		{
			out_AbsByte(0xC9);
		}
		| T_Z80_RET ccode
		{
			out_AbsByte(0xC0 | ($2 << 3));
		}
;

z80_reti	: T_Z80_RETI
		{
			out_AbsByte(0xD9);
		}
;

z80_rl		: T_Z80_RL reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x10 | $2);
		}
;

z80_rla		: T_Z80_RLA
		{
			out_AbsByte(0x17);
		}
;

z80_rlc		: T_Z80_RLC reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x00 | $2);
		}
;

z80_rlca	: T_Z80_RLCA
		{
			out_AbsByte(0x07);
		}
;

z80_rr		: T_Z80_RR reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x18 | $2);
		}
;

z80_rra		: T_Z80_RRA
		{
			out_AbsByte(0x1F);
		}
;

z80_rrc		: T_Z80_RRC reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x08 | $2);
		}
;

z80_rrca	: T_Z80_RRCA
		{
			out_AbsByte(0x0F);
		}
;

z80_rst		: T_Z80_RST reloc_8bit
		{
			if (!rpn_isKnown(&$2)) {
				rpn_CheckRST(&$2, &$2);
				out_RelByte(&$2);
			} else if (($2.nVal & 0x38) != $2.nVal) {
				yyerror("Invalid address $%x for RST", $2.nVal);
			} else {
				out_AbsByte(0xC7 | $2.nVal);
			}
			rpn_Free(&$2);
		}
;

z80_sbc		: T_Z80_SBC op_a_n
		{
			out_AbsByte(0xDE);
			out_RelByte(&$2);
		}
		| T_Z80_SBC op_a_r
		{
			out_AbsByte(0x98 | $2);
		}
;

z80_scf		: T_Z80_SCF
		{
			out_AbsByte(0x37);
		}
;

z80_set		: T_POP_SET const_3bit comma reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0xC0 | ($2 << 3) | $4);
		}
;

z80_sla		: T_Z80_SLA reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x20 | $2);
		}
;

z80_sra		: T_Z80_SRA reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x28 | $2);
		}
;

z80_srl		: T_Z80_SRL reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x38 | $2);
		}
;

z80_stop	: T_Z80_STOP
		{
			out_AbsByte(0x10);
			out_AbsByte(0x00);
		}
		| T_Z80_STOP reloc_8bit
		{
			out_AbsByte(0x10);
			out_RelByte(&$2);
		}
;

z80_sub		: T_Z80_SUB op_a_n
		{
			out_AbsByte(0xD6);
			out_RelByte(&$2);
		}
		| T_Z80_SUB op_a_r
		{
			out_AbsByte(0x90 | $2);
		}
;

z80_swap	: T_Z80_SWAP reg_r
		{
			out_AbsByte(0xCB);
			out_AbsByte(0x30 | $2);
		}
;

z80_xor		: T_Z80_XOR op_a_n
		{
			out_AbsByte(0xEE);
			out_RelByte(&$2);
		}
		| T_Z80_XOR op_a_r
		{
			out_AbsByte(0xA8 | $2);
		}
;

op_mem_ind	: '[' reloc_16bit ']'		{ $$ = $2; }
;

op_hl_ss	: reg_ss			{ $$ = $1; }
		| T_MODE_HL comma reg_ss	{ $$ = $3; }
;

op_a_r		: reg_r				{ $$ = $1; }
		| T_MODE_A comma reg_r		{ $$ = $3; }
;

op_a_n		: reloc_8bit			{ $$ = $1; }
		| T_MODE_A comma reloc_8bit	{ $$ = $3; }
;

comma		: ','
;

T_MODE_A	: T_TOKEN_A
		| T_OP_HIGH '(' T_MODE_AF ')'
;

T_MODE_B	: T_TOKEN_B
		| T_OP_HIGH '(' T_MODE_BC ')'
;

T_MODE_C	: T_TOKEN_C
		| T_OP_LOW '(' T_MODE_BC ')'
;

T_MODE_D	: T_TOKEN_D
		| T_OP_HIGH '(' T_MODE_DE ')'
;

T_MODE_E	: T_TOKEN_E
		| T_OP_LOW '(' T_MODE_DE ')'
;

T_MODE_H	: T_TOKEN_H
		| T_OP_HIGH '(' T_MODE_HL ')'
;

T_MODE_L	: T_TOKEN_L
		| T_OP_LOW '(' T_MODE_HL ')'
;

ccode		: T_CC_NZ		{ $$ = CC_NZ; }
		| T_CC_Z		{ $$ = CC_Z; }
		| T_CC_NC		{ $$ = CC_NC; }
		| T_TOKEN_C		{ $$ = CC_C; }
;

reg_r		: T_MODE_B		{ $$ = REG_B; }
		| T_MODE_C		{ $$ = REG_C; }
		| T_MODE_D		{ $$ = REG_D; }
		| T_MODE_E		{ $$ = REG_E; }
		| T_MODE_H		{ $$ = REG_H; }
		| T_MODE_L		{ $$ = REG_L; }
		| T_MODE_HL_IND		{ $$ = REG_HL_IND; }
		| T_MODE_A		{ $$ = REG_A; }
;

reg_tt		: T_MODE_BC		{ $$ = REG_BC; }
		| T_MODE_DE		{ $$ = REG_DE; }
		| T_MODE_HL		{ $$ = REG_HL; }
		| T_MODE_AF		{ $$ = REG_AF; }
;

reg_ss		: T_MODE_BC		{ $$ = REG_BC; }
		| T_MODE_DE		{ $$ = REG_DE; }
		| T_MODE_HL		{ $$ = REG_HL; }
		| T_MODE_SP		{ $$ = REG_SP; }
;

reg_rr		: T_MODE_BC_IND		{ $$ = REG_BC_IND; }
		| T_MODE_DE_IND		{ $$ = REG_DE_IND; }
		| T_MODE_HL_INDINC	{ $$ = REG_HL_INDINC; }
		| T_MODE_HL_INDDEC	{ $$ = REG_HL_INDDEC; }
;

%%
