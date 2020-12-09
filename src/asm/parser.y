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
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/macro.h"
#include "asm/main.h"
#include "asm/mymath.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/util.h"
#include "asm/warning.h"

#include "extern/utf8decoder.h"

#include "linkdefs.h"
#include "platform.h" // strncasecmp, strdup

uint32_t nListCountEmpty;
int32_t nPCOffset;
bool executeElseBlock; /* If this is set, ELIFs cannot be executed anymore */

static uint32_t str2int2(uint8_t *s, int32_t length)
{
	int32_t i;
	uint32_t r = 0;

	i = length < 4 ? 0 : length - 4;
	while (i < length) {
		r <<= 8;
		r |= s[i];
		i++;
	}

	return r;
}

static size_t strlenUTF8(const char *s)
{
	size_t len = 0;
	uint32_t state = 0;
	uint32_t codep = 0;

	while (*s) {
		switch (decode(&state, &codep, *s)) {
		case 1:
			fatalerror("STRLEN: Invalid UTF-8 character\n");
			break;
		case 0:
			len++;
			break;
		}
		s++;
	}

	/* Check for partial code point. */
	if (state != 0)
		fatalerror("STRLEN: Invalid UTF-8 character\n");

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
		warning(WARNING_BUILTIN_ARG, "STRSUB: Position starts at 1\n");
		pos = 1;
	}

	/* Advance to starting position in source string. */
	while (src[srcIndex] && curPos < pos) {
		switch (decode(&state, &codep, src[srcIndex])) {
		case 1:
			fatalerror("STRSUB: Invalid UTF-8 character\n");
			break;
		case 0:
			curPos++;
			break;
		}
		srcIndex++;
	}

	if (!src[srcIndex] && len)
		warning(WARNING_BUILTIN_ARG,
			"STRSUB: Position %lu is past the end of the string\n",
			(unsigned long)pos);

	/* Copy from source to destination. */
	while (src[srcIndex] && destIndex < MAXSTRLEN && curLen < len) {
		switch (decode(&state, &codep, src[srcIndex])) {
		case 1:
			fatalerror("STRSUB: Invalid UTF-8 character\n");
			break;
		case 0:
			curLen++;
			break;
		}
		dest[destIndex++] = src[srcIndex++];
	}

	if (curLen < len)
		warning(WARNING_BUILTIN_ARG, "STRSUB: Length too big: %lu\n", (unsigned long)len);

	/* Check for partial code point. */
	if (state != 0)
		fatalerror("STRSUB: Invalid UTF-8 character\n");

	dest[destIndex] = 0;
}

static inline void failAssert(enum AssertionType type)
{
	switch (type) {
		case ASSERT_FATAL:
			fatalerror("Assertion failed\n");
		case ASSERT_ERROR:
			error("Assertion failed\n");
			break;
		case ASSERT_WARN:
			warning(WARNING_ASSERT, "Assertion failed\n");
			break;
	}
}

static inline void failAssertMsg(enum AssertionType type, char const *msg)
{
	switch (type) {
		case ASSERT_FATAL:
			fatalerror("Assertion failed: %s\n", msg);
		case ASSERT_ERROR:
			error("Assertion failed: %s\n", msg);
			break;
		case ASSERT_WARN:
			warning(WARNING_ASSERT, "Assertion failed: %s\n", msg);
			break;
	}
}

#define yyerror(str) error(str "\n")

%}

%union
{
	char tzSym[MAXSYMLEN + 1];
	char tzString[MAXSTRLEN + 1];
	struct Expression sVal;
	int32_t nConstValue;
	enum SectionModifier sectMod;
	struct SectionSpec sectSpec;
	struct MacroArgs *macroArg;
	enum AssertionType assertType;
}

%type	<sVal>		relocexpr
%type	<sVal>		relocexpr_no_str
%type	<nConstValue>	const
%type	<nConstValue>	uconst
%type	<nConstValue>	rs_uconst
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

%left	T_OP_LOGICNOT
%left	T_OP_LOGICOR T_OP_LOGICAND
%left	T_OP_LOGICGT T_OP_LOGICLT T_OP_LOGICGE T_OP_LOGICLE T_OP_LOGICNE T_OP_LOGICEQU
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

%token	T_OP_HIGH T_OP_LOW
%token	T_OP_ISCONST

%left	T_OP_STRCMP
%left	T_OP_STRIN
%left	T_OP_STRSUB
%left	T_OP_STRLEN
%left	T_OP_STRCAT
%left	T_OP_STRUPR
%left	T_OP_STRLWR

%left	NEG /* negation -- unary minus */

%token	<tzSym> T_LABEL
%token	<tzSym> T_ID
%token	<tzSym> T_LOCAL_ID
%type	<tzSym> scoped_id
%token	T_POP_EQU
%token	T_POP_SET
%token	T_POP_EQUAL
%token	T_POP_EQUS

%token	T_POP_INCLUDE T_POP_PRINTF T_POP_PRINTT T_POP_PRINTV T_POP_PRINTI
%token	T_POP_IF T_POP_ELIF T_POP_ELSE T_POP_ENDC
%token	T_POP_EXPORT T_POP_GLOBAL T_POP_XDEF
%token	T_POP_DB T_POP_DS T_POP_DW T_POP_DL
%token	T_POP_SECTION T_POP_FRAGMENT
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
%token	T_POP_FATAL
%token	T_POP_ASSERT T_POP_STATIC_ASSERT
%token	T_POP_PURGE
%token	T_POP_POPS
%token	T_POP_PUSHS
%token	T_POP_POPO
%token	T_POP_PUSHO
%token	T_POP_OPT
%token	T_SECT_WRAM0 T_SECT_VRAM T_SECT_ROMX T_SECT_ROM0 T_SECT_HRAM
%token	T_SECT_WRAMX T_SECT_SRAM T_SECT_OAM

%type	<sectMod> sectmod
%type	<macroArg> macroargs

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
%token	T_MODE_BC
%token	T_MODE_DE
%token	T_MODE_SP
%token	T_MODE_HW_C
%token	T_MODE_HL T_MODE_HL_DEC T_MODE_HL_INC
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
%type	<assertType>	assert_type
%start asmfile

%%

asmfile		: lines;

/* Note: The lexer adds '\n' at the end of the input */
lines		: /* empty */
		| lines {
			nListCountEmpty = 0;
			nPCOffset = 0;
		} line {
			nTotalLines++;
		}
;

line		: label '\n'
		| label cpu_command '\n'
		| label macro '\n'
		| label simple_pseudoop '\n'
		| pseudoop '\n'
		| conditional /* May not necessarily be followed by a newline, see below */
;

/*
 * For "logistical" reasons, conditionals must manage newlines themselves.
 * This is because we need to switch the lexer's mode *after* the newline has been read,
 * and to avoid causing some grammar conflicts (token reducing is finicky).
 * This is DEFINITELY one of the more FRAGILE parts of the codebase, handle with care.
 */
conditional	: if
		/* It's important that all of these require being at line start for `skipIfBlock` */
		| elif
		| else
		| endc
;

if		: T_POP_IF const '\n' {
			nIFDepth++;
			executeElseBlock = !$2;
			if (executeElseBlock)
				lexer_SetMode(LEXER_SKIP_TO_ELIF);
		}
;

elif		: T_POP_ELIF const '\n' {
			if (nIFDepth <= 0)
				fatalerror("Found ELIF outside an IF construct\n");

			if (!executeElseBlock) {
				lexer_SetMode(LEXER_SKIP_TO_ENDC);
			} else {
				executeElseBlock = !$2;
				if (executeElseBlock)
					lexer_SetMode(LEXER_SKIP_TO_ELIF);
			}
		}
;

else		: T_POP_ELSE '\n' {
			if (nIFDepth <= 0)
				fatalerror("Found ELSE outside an IF construct\n");

			if (!executeElseBlock)
				lexer_SetMode(LEXER_SKIP_TO_ENDC);
		}
;

endc		: T_POP_ENDC '\n' {
			if (nIFDepth <= 0)
				fatalerror("Found ENDC outside an IF construct\n");

			nIFDepth--;
			executeElseBlock = false;
		}
;

scoped_id	: T_ID | T_LOCAL_ID ;

label		: /* empty */
		| T_LOCAL_ID {
			sym_AddLocalLabel($1);
		}
		| T_LABEL {
			warning(WARNING_OBSOLETE, "Non-local labels without a colon are deprecated\n");
			sym_AddLabel($1);
		}
		| T_LOCAL_ID ':' {
			sym_AddLocalLabel($1);
		}
		| T_LABEL ':' {
			sym_AddLabel($1);
		}
		| T_LOCAL_ID ':' ':' {
			sym_AddLocalLabel($1);
			sym_Export($1);
		}
		| T_LABEL ':' ':' {
			sym_AddLabel($1);
			sym_Export($1);
		}
;

macro		: T_ID {
			lexer_SetMode(LEXER_RAW);
		} macroargs {
			lexer_SetMode(LEXER_NORMAL);
			fstk_RunMacro($1, $3);
		}
;

macroargs	: /* empty */ {
			$$ = macro_NewArgs();
		}
		| T_STRING {
			$$ = macro_NewArgs();
			macro_AppendArg(&($$), strdup($1));
		}
		| macroargs ',' T_STRING {
			macro_AppendArg(&($$), strdup($3));
		}
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
		| export
		| db
		| dw
		| dl
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
		| assert
		| purge
		| pops
		| pushs
		| popo
		| pusho
		| opt
		| align
;

align		: T_OP_ALIGN uconst {
			if ($2 > 16)
				error("Alignment must be between 0 and 16, not %u\n", $2);
			else
				sect_AlignPC($2, 0);
		}
		| T_OP_ALIGN uconst ',' uconst {
			if ($2 > 16)
				error("Alignment must be between 0 and 16, not %u\n", $2);
			else if ($4 >= 1 << $2)
				error("Offset must be between 0 and %u, not %u\n",
					(1 << $2) - 1, $4);
			else
				sect_AlignPC($2, $4);
		}
;

opt		: T_POP_OPT {
			lexer_SetMode(LEXER_RAW);
		} opt_list {
			lexer_SetMode(LEXER_NORMAL);
		}
;

opt_list	: opt_list_entry
		| opt_list ',' opt_list_entry
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

fail		: T_POP_FAIL string	{ fatalerror("%s\n", $2); }
;

warn		: T_POP_WARN string	{ warning(WARNING_USER, "%s\n", $2); }
;

assert_type	: /* empty */		{ $$ = ASSERT_ERROR; }
		| T_POP_WARN ','	{ $$ = ASSERT_WARN; }
		| T_POP_FAIL ','	{ $$ = ASSERT_ERROR; }
		| T_POP_FATAL ','	{ $$ = ASSERT_FATAL; }
;

assert		: T_POP_ASSERT assert_type relocexpr
		{
			if (!rpn_isKnown(&$3)) {
				if (!out_CreateAssert($2, &$3, "",
						      sect_GetOutputOffset()))
					error("Assertion creation failed: %s\n",
						strerror(errno));
			} else if ($3.nVal == 0) {
				failAssert($2);
			}
			rpn_Free(&$3);
		}
		| T_POP_ASSERT assert_type relocexpr ',' string
		{
			if (!rpn_isKnown(&$3)) {
				if (!out_CreateAssert($2, &$3, $5,
						      sect_GetOutputOffset()))
					error("Assertion creation failed: %s\n",
						strerror(errno));
			} else if ($3.nVal == 0) {
				failAssertMsg($2, $5);
			}
			rpn_Free(&$3);
		}
		| T_POP_STATIC_ASSERT assert_type const
		{
			if ($3 == 0)
				failAssert($2);
		}
		| T_POP_STATIC_ASSERT assert_type const ',' string
		{
			if ($3 == 0)
				failAssertMsg($2, $5);
		}
;

shift		: T_POP_SHIFT		{ macro_ShiftCurrentArgs(1); }
		| T_POP_SHIFT const	{ macro_ShiftCurrentArgs($2); }
;

load		: T_POP_LOAD string ',' sectiontype sectorg sectattrs {
			out_SetLoadSection($2, $4, $5, &$6);
		}
		| T_POP_ENDL	{ out_EndLoadSection(); }
;

rept		: T_POP_REPT uconst {
			uint32_t nDefinitionLineNo = lexer_GetLineNo();
			char *body;
			size_t size;
			lexer_CaptureRept(&body, &size);
			fstk_RunRept($2, nDefinitionLineNo, body, size);
		}
;

macrodef	: T_LABEL ':' T_POP_MACRO {
			int32_t nDefinitionLineNo = lexer_GetLineNo();
			char *body;
			size_t size;
			lexer_CaptureMacroBody(&body, &size);
			sym_AddMacro($1, nDefinitionLineNo, body, size);
		}
;

equs		: T_LABEL T_POP_EQUS string	{ sym_AddString($1, $3); }
;

rsset		: T_POP_RSSET uconst	{ sym_AddSet("_RS", $2); }
;

rsreset		: T_POP_RSRESET	{ sym_AddSet("_RS", 0); }
;

rs_uconst	: /* empty */ {
			$$ = 1;
		}
		| uconst {
			$$ = $1;
		}
;

rl		: T_LABEL T_POP_RL rs_uconst {
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + 4 * $3);
		}
;

rw		: T_LABEL T_POP_RW rs_uconst {
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + 2 * $3);
		}
;

rb		: T_LABEL T_POP_RB rs_uconst {
			sym_AddEqu($1, sym_GetConstantValue("_RS"));
			sym_AddSet("_RS", sym_GetConstantValue("_RS") + $3);
		}
;

union		: T_POP_UNION	{ sect_StartUnion(); }
;

nextu		: T_POP_NEXTU	{ sect_NextUnionMember(); }
;

endu		: T_POP_ENDU	{ sect_EndUnion(); }
;

ds		: T_POP_DS uconst	{ out_Skip($2, true); }
		| T_POP_DS uconst ',' reloc_8bit {
			out_RelBytes(&$4, $2);
		}
;

/* Authorize empty entries if there is only one */
db		: T_POP_DB constlist_8bit_entry ',' constlist_8bit {
			if (nListCountEmpty > 0)
				warning(WARNING_EMPTY_ENTRY,
					"Empty entry in list of 8-bit elements (treated as padding).\n");
		}
		| T_POP_DB constlist_8bit_entry
;

dw		: T_POP_DW constlist_16bit_entry ',' constlist_16bit {
			if (nListCountEmpty > 0)
				warning(WARNING_EMPTY_ENTRY,
					"Empty entry in list of 16-bit elements (treated as padding).\n");
		}
		| T_POP_DW constlist_16bit_entry
;

dl		: T_POP_DL constlist_32bit_entry ',' constlist_32bit {
			if (nListCountEmpty > 0)
				warning(WARNING_EMPTY_ENTRY,
					"Empty entry in list of 32-bit elements (treated as padding).\n");
		}
		| T_POP_DL constlist_32bit_entry
;

purge		: T_POP_PURGE {
			lexer_ToggleStringExpansion(false);
		} purge_list {
			lexer_ToggleStringExpansion(true);
		}
;

purge_list	: purge_list_entry
		| purge_list ',' purge_list_entry
;

purge_list_entry : scoped_id	{ sym_Purge($1); }
;

export		: export_token export_list
;

export_token	: T_POP_EXPORT
		| T_POP_GLOBAL {
			warning(WARNING_OBSOLETE,
				"`GLOBAL` is a deprecated synonym for `EXPORT`\n");
		}
		| T_POP_XDEF {
			warning(WARNING_OBSOLETE, "`XDEF` is a deprecated synonym for `EXPORT`\n");
		}
;

export_list	: export_list_entry
		| export_list ',' export_list_entry
;

export_list_entry : scoped_id	{ sym_Export($1); }
;

equ		: T_LABEL T_POP_EQU const	{ sym_AddEqu($1, $3); }
;

set		: T_LABEL T_POP_SET const	{ sym_AddSet($1, $3); }
		| T_LABEL T_POP_EQUAL const	{ sym_AddSet($1, $3); }
;

include		: T_POP_INCLUDE string {
			fstk_RunInclude($2);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
;

incbin		: T_POP_INCBIN string {
			out_BinaryFile($2, 0);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
		| T_POP_INCBIN string ',' const {
			out_BinaryFile($2, $4);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
		| T_POP_INCBIN string ',' const ',' const {
			out_BinaryFileSlice($2, $4, $6);
			if (oFailedOnMissingInclude)
				YYACCEPT;
		}
;

charmap		: T_POP_CHARMAP string ',' const {
			if ($4 < INT8_MIN || $4 > UINT8_MAX)
				warning(WARNING_TRUNCATION, "Expression must be 8-bit\n");
			charmap_Add($2, (uint8_t)$4);
		}
;

newcharmap	: T_POP_NEWCHARMAP T_ID	{ charmap_New($2, NULL); }
		| T_POP_NEWCHARMAP T_ID ',' T_ID	{ charmap_New($2, $4); }
;

setcharmap	: T_POP_SETCHARMAP T_ID	{ charmap_Set($2); }
;

pushc		: T_POP_PUSHC	{ charmap_Push(); }
;

popc		: T_POP_POPC	{ charmap_Pop(); }
;

printt		: T_POP_PRINTT string	{ printf("%s", $2); }
;

printv		: T_POP_PRINTV const	{ printf("$%" PRIX32, $2); }
;

printi		: T_POP_PRINTI const	{ printf("%" PRId32, $2); }
;

printf		: T_POP_PRINTF const	{ math_Print($2); }
;

const_3bit	: const {
			int32_t value = $1;

			if ((value < 0) || (value > 7)) {
				error("Immediate value must be 3-bit\n");
				$$ = 0;
			} else {
				$$ = value & 0x7;
			}
		}
;

constlist_8bit	: constlist_8bit_entry
		| constlist_8bit ',' constlist_8bit_entry
;

constlist_8bit_entry : /* empty */ {
			out_Skip(1, false);
			nListCountEmpty++;
		}
		| reloc_8bit_no_str	{ out_RelByte(&$1); }
		| string {
			uint8_t *output = malloc(strlen($1)); /* Cannot be larger than that */
			int32_t length = charmap_Convert($1, output);

			out_AbsByteGroup(output, length);
			free(output);
		}
;

constlist_16bit : constlist_16bit_entry
		| constlist_16bit ',' constlist_16bit_entry
;

constlist_16bit_entry : /* empty */ {
			out_Skip(2, false);
			nListCountEmpty++;
		}
		| reloc_16bit	{ out_RelWord(&$1); }
;

constlist_32bit : constlist_32bit_entry
		| constlist_32bit ',' constlist_32bit_entry
;

constlist_32bit_entry : /* empty */ {
			out_Skip(4, false);
			nListCountEmpty++;
		}
		| relocexpr	{ out_RelLong(&$1); }
;

reloc_8bit	: relocexpr {
			if(rpn_isKnown(&$1)
			 && ($1.nVal < -128 || $1.nVal > 255))
				warning(WARNING_TRUNCATION, "Expression must be 8-bit\n");
			$$ = $1;
		}
;

reloc_8bit_no_str : relocexpr_no_str {
			if(rpn_isKnown(&$1)
			 && ($1.nVal < -128 || $1.nVal > 255))
				warning(WARNING_TRUNCATION, "Expression must be 8-bit\n");
			$$ = $1;
		}
;

reloc_16bit	: relocexpr {
			if (rpn_isKnown(&$1)
			 && ($1.nVal < -32768 || $1.nVal > 65535))
				warning(WARNING_TRUNCATION, "Expression must be 16-bit\n");
			$$ = $1;
		}
;


relocexpr	: relocexpr_no_str
		| string {
			uint8_t *output = malloc(strlen($1)); /* Cannot be longer than that */
			int32_t length = charmap_Convert($1, output);
			uint32_t r = str2int2(output, length);

			free(output);
			rpn_Number(&$$, r);
		}
;

relocexpr_no_str : scoped_id	{ rpn_Symbol(&$$, $1); }
		| T_NUMBER	{ rpn_Number(&$$, $1); }
		| T_OP_LOGICNOT relocexpr %prec NEG {
			rpn_LOGNOT(&$$, &$2);
		}
		| relocexpr T_OP_LOGICOR relocexpr {
			rpn_BinaryOp(RPN_LOGOR, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICAND relocexpr {
			rpn_BinaryOp(RPN_LOGAND, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICEQU relocexpr {
			rpn_BinaryOp(RPN_LOGEQ, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICGT relocexpr {
			rpn_BinaryOp(RPN_LOGGT, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICLT relocexpr {
			rpn_BinaryOp(RPN_LOGLT, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICGE relocexpr {
			rpn_BinaryOp(RPN_LOGGE, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICLE relocexpr {
			rpn_BinaryOp(RPN_LOGLE, &$$, &$1, &$3);
		}
		| relocexpr T_OP_LOGICNE relocexpr {
			rpn_BinaryOp(RPN_LOGNE, &$$, &$1, &$3);
		}
		| relocexpr T_OP_ADD relocexpr {
			rpn_BinaryOp(RPN_ADD, &$$, &$1, &$3);
		}
		| relocexpr T_OP_SUB relocexpr {
			rpn_BinaryOp(RPN_SUB, &$$, &$1, &$3);
		}
		| relocexpr T_OP_XOR relocexpr {
			rpn_BinaryOp(RPN_XOR, &$$, &$1, &$3);
		}
		| relocexpr T_OP_OR relocexpr	 {
			rpn_BinaryOp(RPN_OR, &$$, &$1, &$3);
		}
		| relocexpr T_OP_AND relocexpr {
			rpn_BinaryOp(RPN_AND, &$$, &$1, &$3);
		}
		| relocexpr T_OP_SHL relocexpr {
			rpn_BinaryOp(RPN_SHL, &$$, &$1, &$3);
		}
		| relocexpr T_OP_SHR relocexpr {
			rpn_BinaryOp(RPN_SHR, &$$, &$1, &$3);
		}
		| relocexpr T_OP_MUL relocexpr {
			rpn_BinaryOp(RPN_MUL, &$$, &$1, &$3);
		}
		| relocexpr T_OP_DIV relocexpr {
			rpn_BinaryOp(RPN_DIV, &$$, &$1, &$3);
		}
		| relocexpr T_OP_MOD relocexpr {
			rpn_BinaryOp(RPN_MOD, &$$, &$1, &$3);
		}
		| T_OP_ADD relocexpr %prec NEG	{ $$ = $2; }
		| T_OP_SUB relocexpr %prec NEG	{ rpn_UNNEG(&$$, &$2); }
		| T_OP_NOT relocexpr %prec NEG	{ rpn_UNNOT(&$$, &$2); }
		| T_OP_HIGH '(' relocexpr ')'	{ rpn_HIGH(&$$, &$3); }
		| T_OP_LOW '(' relocexpr ')'	{ rpn_LOW(&$$, &$3); }
		| T_OP_ISCONST '(' relocexpr ')'{ rpn_ISCONST(&$$, &$3); }
		| T_OP_BANK '(' scoped_id ')' {
			/* '@' is also a T_ID, it is handled here. */
			rpn_BankSymbol(&$$, $3);
		}
		| T_OP_BANK '(' string ')'	{ rpn_BankSection(&$$, $3); }
		| T_OP_DEF {
			lexer_ToggleStringExpansion(false);
		} '(' scoped_id ')' {
			struct Symbol const *sym = sym_FindScopedSymbol($4);

			rpn_Number(&$$, !!sym);

			lexer_ToggleStringExpansion(true);
		}
		| T_OP_ROUND '(' const ')' {
			rpn_Number(&$$, math_Round($3));
		}
		| T_OP_CEIL '(' const ')' {
			rpn_Number(&$$, math_Ceil($3));
		}
		| T_OP_FLOOR '(' const ')' {
			rpn_Number(&$$, math_Floor($3));
		}
		| T_OP_FDIV '(' const ',' const ')' {
			rpn_Number(&$$, math_Div($3, $5));
		}
		| T_OP_FMUL '(' const ',' const ')' {
			rpn_Number(&$$, math_Mul($3, $5));
		}
		| T_OP_SIN '(' const ')' {
			rpn_Number(&$$, math_Sin($3));
		}
		| T_OP_COS '(' const ')' {
			rpn_Number(&$$, math_Cos($3));
		}
		| T_OP_TAN '(' const ')' {
			rpn_Number(&$$, math_Tan($3));
		}
		| T_OP_ASIN '(' const ')' {
			rpn_Number(&$$, math_ASin($3));
		}
		| T_OP_ACOS '(' const ')' {
			rpn_Number(&$$, math_ACos($3));
		}
		| T_OP_ATAN '(' const ')' {
			rpn_Number(&$$, math_ATan($3));
		}
		| T_OP_ATAN2 '(' const ',' const ')' {
			rpn_Number(&$$, math_ATan2($3, $5));
		}
		| T_OP_STRCMP '(' string ',' string ')' {
			rpn_Number(&$$, strcmp($3, $5));
		}
		| T_OP_STRIN '(' string ',' string ')' {
			char *p = strstr($3, $5);

			rpn_Number(&$$, p ? p - $3 + 1 : 0);
		}
		| T_OP_STRLEN '(' string ')' {
			rpn_Number(&$$, strlenUTF8($3));
		}
		| '(' relocexpr ')'	{ $$ = $2; }
;

uconst		: const {
			$$ = $1;
			if ($$ < 0)
				fatalerror("Constant mustn't be negative: %d\n",
					   $1);
		}
;

const		: relocexpr {
			if (!rpn_isKnown(&$1)) {
				error("Expected constant expression: %s\n",
					$1.reason);
				$$ = 0;
			} else {
				$$ = $1.nVal;
			}
		}
;

string		: T_STRING {
			if (snprintf($$, MAXSTRLEN + 1, "%s", $1) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "String is too long '%s'\n", $1);
		}
		| T_OP_STRSUB '(' string ',' uconst ',' uconst ')' {
			strsubUTF8($$, $3, $5, $7);
		}
		| T_OP_STRCAT '(' string ',' string ')' {
			if (snprintf($$, MAXSTRLEN + 1, "%s%s", $3, $5) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRCAT: String too long '%s%s'\n",
					$3, $5);
		}
		| T_OP_STRUPR '(' string ')' {
			if (snprintf($$, MAXSTRLEN + 1, "%s", $3) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRUPR: String too long '%s'\n", $3);

			upperstring($$);
		}
		| T_OP_STRLWR '(' string ')' {
			if (snprintf($$, MAXSTRLEN + 1, "%s", $3) > MAXSTRLEN)
				warning(WARNING_LONG_STR, "STRUPR: String too long '%s'\n", $3);

			lowerstring($$);
		}
;

section		: T_POP_SECTION sectmod string ',' sectiontype sectorg sectattrs {
			out_NewSection($3, $5, $6, &$7, $2);
		}
;

sectmod	: /* empty */	{ $$ = SECTION_NORMAL; }
		| T_POP_UNION	{ $$ = SECTION_UNION; }
		| T_POP_FRAGMENT{ $$ = SECTION_FRAGMENT; }
;

sectiontype	: T_SECT_WRAM0	{ $$ = SECTTYPE_WRAM0; }
		| T_SECT_VRAM	{ $$ = SECTTYPE_VRAM; }
		| T_SECT_ROMX	{ $$ = SECTTYPE_ROMX; }
		| T_SECT_ROM0	{ $$ = SECTTYPE_ROM0; }
		| T_SECT_HRAM	{ $$ = SECTTYPE_HRAM; }
		| T_SECT_WRAMX	{ $$ = SECTTYPE_WRAMX; }
		| T_SECT_SRAM	{ $$ = SECTTYPE_SRAM; }
		| T_SECT_OAM	{ $$ = SECTTYPE_OAM; }
;

sectorg		: /* empty */ { $$ = -1; }
		| '[' uconst ']' {
			if ($2 < 0 || $2 >= 0x10000) {
				error("Address $%x is not 16-bit\n", $2);
				$$ = -1;
			} else {
				$$ = $2;
			}
		}
;

sectattrs	: /* empty */ {
			$$.alignment = 0;
			$$.alignOfs = 0;
			$$.bank = -1;
		}
		| sectattrs ',' T_OP_ALIGN '[' uconst ']' {
			if ($5 > 16)
				error("Alignment must be between 0 and 16, not %u\n", $5);
			else
				$$.alignment = $5;
		}
		| sectattrs ',' T_OP_ALIGN '[' uconst ',' uconst ']' {
			if ($5 > 16) {
				error("Alignment must be between 0 and 16, not %u\n", $5);
			} else {
				$$.alignment = $5;
				if ($7 >= 1 << $$.alignment)
					error("Alignment offset must not be greater than alignment (%u < %u)\n",
						$7, 1 << $$.alignment);
				else
					$$.alignOfs = $7;
			}
		}
		| sectattrs ',' T_OP_BANK '[' uconst ']' {
			/* We cannot check the validity of this now */
			$$.bank = $5;
		}
;


cpu_command	: { nPCOffset = 1; } z80_adc
		| { nPCOffset = 1; } z80_add
		| { nPCOffset = 1; } z80_and
		| { nPCOffset = 1; } z80_bit
		| { nPCOffset = 1; } z80_call
		| z80_ccf
		| { nPCOffset = 1; } z80_cp
		| z80_cpl
		| z80_daa
		| { nPCOffset = 1; } z80_dec
		| z80_di
		| z80_ei
		| z80_halt
		| z80_inc
		| { nPCOffset = 1; } z80_jp
		| { nPCOffset = 1; } z80_jr
		| { nPCOffset = 1; } z80_ld
		| z80_ldd
		| z80_ldi
		| { nPCOffset = 1; } z80_ldio
		| z80_nop
		| { nPCOffset = 1; } z80_or
		| z80_pop
		| z80_push
		| { nPCOffset = 1; } z80_res
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
		| /*{ nPCOffset = 0; }*/ z80_rst
		| { nPCOffset = 1; } z80_sbc
		| z80_scf
		| { nPCOffset = 1; } z80_set
		| z80_sla
		| z80_sra
		| z80_srl
		| { nPCOffset = 1; } z80_stop
		| { nPCOffset = 1; } z80_sub
		| z80_swap
		| { nPCOffset = 1; } z80_xor
;

z80_adc		: T_Z80_ADC op_a_n {
			out_AbsByte(0xCE);
			out_RelByte(&$2);
		}
		| T_Z80_ADC op_a_r	{ out_AbsByte(0x88 | $2); }
;

z80_add		: T_Z80_ADD op_a_n {
			out_AbsByte(0xC6);
			out_RelByte(&$2);
		}
		| T_Z80_ADD op_a_r	{ out_AbsByte(0x80 | $2); }
		| T_Z80_ADD op_hl_ss	{ out_AbsByte(0x09 | ($2 << 4)); }
		| T_Z80_ADD T_MODE_SP ',' reloc_8bit {
			out_AbsByte(0xE8);
			out_RelByte(&$4);
		}

;

z80_and		: T_Z80_AND op_a_n {
			out_AbsByte(0xE6);
			out_RelByte(&$2);
		}
		| T_Z80_AND op_a_r	{ out_AbsByte(0xA0 | $2); }
;

z80_bit		: T_Z80_BIT const_3bit ',' reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x40 | ($2 << 3) | $4);
		}
;

z80_call	: T_Z80_CALL reloc_16bit {
			out_AbsByte(0xCD);
			out_RelWord(&$2);
		}
		| T_Z80_CALL ccode ',' reloc_16bit {
			out_AbsByte(0xC4 | ($2 << 3));
			out_RelWord(&$4);
		}
;

z80_ccf		: T_Z80_CCF	{ out_AbsByte(0x3F); }
;

z80_cp		: T_Z80_CP op_a_n {
			out_AbsByte(0xFE);
			out_RelByte(&$2);
		}
		| T_Z80_CP op_a_r	{ out_AbsByte(0xB8 | $2); }
;

z80_cpl		: T_Z80_CPL	{ out_AbsByte(0x2F); }
;

z80_daa		: T_Z80_DAA	{ out_AbsByte(0x27); }
;

z80_dec		: T_Z80_DEC reg_r	{ out_AbsByte(0x05 | ($2 << 3)); }
		| T_Z80_DEC reg_ss	{ out_AbsByte(0x0B | ($2 << 4)); }
;

z80_di		: T_Z80_DI	{ out_AbsByte(0xF3); }
;

z80_ei		: T_Z80_EI	{ out_AbsByte(0xFB); }
;

z80_halt	: T_Z80_HALT {
			out_AbsByte(0x76);
			if (haltnop)
				out_AbsByte(0x00);
		}
;

z80_inc		: T_Z80_INC reg_r	{ out_AbsByte(0x04 | ($2 << 3)); }
		| T_Z80_INC reg_ss	{ out_AbsByte(0x03 | ($2 << 4)); }
;

z80_jp		: T_Z80_JP reloc_16bit {
			out_AbsByte(0xC3);
			out_RelWord(&$2);
		}
		| T_Z80_JP ccode ',' reloc_16bit {
			out_AbsByte(0xC2 | ($2 << 3));
			out_RelWord(&$4);
		}
		| T_Z80_JP T_MODE_HL {
			out_AbsByte(0xE9);
		}
;

z80_jr		: T_Z80_JR reloc_16bit {
			out_AbsByte(0x18);
			out_PCRelByte(&$2);
		}
		| T_Z80_JR ccode ',' reloc_16bit {
			out_AbsByte(0x20 | ($2 << 3));
			out_PCRelByte(&$4);
		}
;

z80_ldi		: T_Z80_LDI '[' T_MODE_HL ']' ',' T_MODE_A {
			out_AbsByte(0x02 | (2 << 4));
		}
		| T_Z80_LDI T_MODE_A ',' '[' T_MODE_HL ']' {
			out_AbsByte(0x0A | (2 << 4));
		}
;

z80_ldd		: T_Z80_LDD '[' T_MODE_HL ']' ',' T_MODE_A {
			out_AbsByte(0x02 | (3 << 4));
		}
		| T_Z80_LDD T_MODE_A ',' '[' T_MODE_HL ']' {
			out_AbsByte(0x0A | (3 << 4));
		}
;

z80_ldio	: T_Z80_LDIO T_MODE_A ',' op_mem_ind {
			rpn_CheckHRAM(&$4, &$4);

			out_AbsByte(0xF0);
			out_RelByte(&$4);
		}
		| T_Z80_LDIO op_mem_ind ',' T_MODE_A {
			rpn_CheckHRAM(&$2, &$2);

			out_AbsByte(0xE0);
			out_RelByte(&$2);
		}
		| T_Z80_LDIO T_MODE_A ',' c_ind {
			out_AbsByte(0xF2);
		}
		| T_Z80_LDIO c_ind ',' T_MODE_A {
			out_AbsByte(0xE2);
		}
;

c_ind		: '[' T_MODE_C ']'
		| '[' T_MODE_HW_C ']'
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

z80_ld_hl	: T_Z80_LD T_MODE_HL ',' T_MODE_SP reloc_8bit {
			out_AbsByte(0xF8);
			out_RelByte(&$5);
		}
		| T_Z80_LD T_MODE_HL ',' reloc_16bit {
			out_AbsByte(0x01 | (REG_HL << 4));
			out_RelWord(&$4);
		}
;

z80_ld_sp	: T_Z80_LD T_MODE_SP ',' T_MODE_HL	{ out_AbsByte(0xF9); }
		| T_Z80_LD T_MODE_SP ',' reloc_16bit {
			out_AbsByte(0x01 | (REG_SP << 4));
			out_RelWord(&$4);
		}
;

z80_ld_mem	: T_Z80_LD op_mem_ind ',' T_MODE_SP {
			out_AbsByte(0x08);
			out_RelWord(&$2);
		}
		| T_Z80_LD op_mem_ind ',' T_MODE_A {
			if (optimizeloads && rpn_isKnown(&$2)
			 && $2.nVal >= 0xFF00) {
				out_AbsByte(0xE0);
				out_AbsByte($2.nVal & 0xFF);
				rpn_Free(&$2);
			} else {
				out_AbsByte(0xEA);
				out_RelWord(&$2);
			}
		}
;

z80_ld_cind	: T_Z80_LD c_ind ',' T_MODE_A {
			out_AbsByte(0xE2);
		}
;

z80_ld_rr	: T_Z80_LD reg_rr ',' T_MODE_A {
			out_AbsByte(0x02 | ($2 << 4));
		}
;

z80_ld_r	: T_Z80_LD reg_r ',' reloc_8bit {
			out_AbsByte(0x06 | ($2 << 3));
			out_RelByte(&$4);
		}
		| T_Z80_LD reg_r ',' reg_r {
			if (($2 == REG_HL_IND) && ($4 == REG_HL_IND))
				error("LD [HL],[HL] not a valid instruction\n");
			else
				out_AbsByte(0x40 | ($2 << 3) | $4);
		}
;

z80_ld_a	: T_Z80_LD reg_r ',' c_ind {
			if ($2 == REG_A)
				out_AbsByte(0xF2);
			else
				error("Destination operand must be A\n");
		}
		| T_Z80_LD reg_r ',' reg_rr {
			if ($2 == REG_A)
				out_AbsByte(0x0A | ($4 << 4));
			else
				error("Destination operand must be A\n");
		}
		| T_Z80_LD reg_r ',' op_mem_ind {
			if ($2 == REG_A) {
				if (optimizeloads && rpn_isKnown(&$4)
				 && $4.nVal >= 0xFF00) {
					out_AbsByte(0xF0);
					out_AbsByte($4.nVal & 0xFF);
					rpn_Free(&$4);
				} else {
					out_AbsByte(0xFA);
					out_RelWord(&$4);
				}
			} else {
				error("Destination operand must be A\n");
				rpn_Free(&$4);
			}
		}
;

z80_ld_ss	: T_Z80_LD T_MODE_BC ',' reloc_16bit {
			out_AbsByte(0x01 | (REG_BC << 4));
			out_RelWord(&$4);
		}
		| T_Z80_LD T_MODE_DE ',' reloc_16bit {
			out_AbsByte(0x01 | (REG_DE << 4));
			out_RelWord(&$4);
		}
		/*
		 * HL is taken care of in z80_ld_hl
		 * SP is taken care of in z80_ld_sp
		 */
;

z80_nop		: T_Z80_NOP	{ out_AbsByte(0x00); }
;

z80_or		: T_Z80_OR op_a_n {
			out_AbsByte(0xF6);
			out_RelByte(&$2);
		}
		| T_Z80_OR op_a_r	{ out_AbsByte(0xB0 | $2); }
;

z80_pop		: T_Z80_POP reg_tt	{ out_AbsByte(0xC1 | ($2 << 4)); }
;

z80_push	: T_Z80_PUSH reg_tt	{ out_AbsByte(0xC5 | ($2 << 4)); }
;

z80_res		: T_Z80_RES const_3bit ',' reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x80 | ($2 << 3) | $4);
		}
;

z80_ret		: T_Z80_RET	{ out_AbsByte(0xC9);
		}
		| T_Z80_RET ccode	{ out_AbsByte(0xC0 | ($2 << 3)); }
;

z80_reti	: T_Z80_RETI	{ out_AbsByte(0xD9); }
;

z80_rl		: T_Z80_RL reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x10 | $2);
		}
;

z80_rla		: T_Z80_RLA	{ out_AbsByte(0x17); }
;

z80_rlc		: T_Z80_RLC reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x00 | $2);
		}
;

z80_rlca	: T_Z80_RLCA	{ out_AbsByte(0x07); }
;

z80_rr		: T_Z80_RR reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x18 | $2);
		}
;

z80_rra		: T_Z80_RRA	{ out_AbsByte(0x1F); }
;

z80_rrc		: T_Z80_RRC reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x08 | $2);
		}
;

z80_rrca	: T_Z80_RRCA	{ out_AbsByte(0x0F); }
;

z80_rst		: T_Z80_RST reloc_8bit {
			rpn_CheckRST(&$2, &$2);
			if (!rpn_isKnown(&$2))
				out_RelByte(&$2);
			else
				out_AbsByte(0xC7 | $2.nVal);
			rpn_Free(&$2);
		}
;

z80_sbc		: T_Z80_SBC op_a_n {
			out_AbsByte(0xDE);
			out_RelByte(&$2);
		}
		| T_Z80_SBC op_a_r	{ out_AbsByte(0x98 | $2); }
;

z80_scf		: T_Z80_SCF	{ out_AbsByte(0x37); }
;

z80_set		: T_POP_SET const_3bit ',' reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0xC0 | ($2 << 3) | $4);
		}
;

z80_sla		: T_Z80_SLA reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x20 | $2);
		}
;

z80_sra		: T_Z80_SRA reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x28 | $2);
		}
;

z80_srl		: T_Z80_SRL reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x38 | $2);
		}
;

z80_stop	: T_Z80_STOP {
			out_AbsByte(0x10);
			out_AbsByte(0x00);
		}
		| T_Z80_STOP reloc_8bit {
			out_AbsByte(0x10);
			out_RelByte(&$2);
		}
;

z80_sub		: T_Z80_SUB op_a_n {
			out_AbsByte(0xD6);
			out_RelByte(&$2);
		}
		| T_Z80_SUB op_a_r	{ out_AbsByte(0x90 | $2);
		}
;

z80_swap	: T_Z80_SWAP reg_r {
			out_AbsByte(0xCB);
			out_AbsByte(0x30 | $2);
		}
;

z80_xor		: T_Z80_XOR op_a_n {
			out_AbsByte(0xEE);
			out_RelByte(&$2);
		}
		| T_Z80_XOR op_a_r	{ out_AbsByte(0xA8 | $2); }
;

op_mem_ind	: '[' reloc_16bit ']'		{ $$ = $2; }
;

op_hl_ss	: reg_ss			{ $$ = $1; }
		| T_MODE_HL ',' reg_ss	{ $$ = $3; }
;

op_a_r		: reg_r				{ $$ = $1; }
		| T_MODE_A ',' reg_r		{ $$ = $3; }
;

op_a_n		: reloc_8bit			{ $$ = $1; }
		| T_MODE_A ',' reloc_8bit	{ $$ = $3; }
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
		| '[' T_MODE_HL ']'	{ $$ = REG_HL_IND; }
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

reg_rr		: '[' T_MODE_BC ']'	{ $$ = REG_BC_IND; }
		| '[' T_MODE_DE ']'	{ $$ = REG_DE_IND; }
		| hl_ind_inc		{ $$ = REG_HL_INDINC; }
		| hl_ind_dec		{ $$ = REG_HL_INDDEC; }
;

hl_ind_inc	: '[' T_MODE_HL_INC ']'
		| '[' T_MODE_HL T_OP_ADD ']'
;

hl_ind_dec	: '[' T_MODE_HL_DEC ']'
		| '[' T_MODE_HL T_OP_SUB ']'
;

%%
