/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/symbol.h"
#include "asm/symbol.h"

#include "helpers.h"

#include "asmy.h"

bool oDontExpandStrings;
int32_t nGBGfxID = -1;
int32_t nBinaryID = -1;

static int32_t gbgfx2bin(char ch)
{
	int32_t i;

	for (i = 0; i <= 3; i += 1) {
		if (CurrentOptions.gbgfx[i] == ch)
			return i;
	}

	return 0;
}

static int32_t binary2bin(char ch)
{
	int32_t i;

	for (i = 0; i <= 1; i += 1) {
		if (CurrentOptions.binary[i] == ch)
			return i;
	}

	return 0;
}

static int32_t char2bin(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);

	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A' + 10);

	if (ch >= '0' && ch <= '9')
		return (ch - '0');

	return 0;
}

typedef int32_t(*x2bin) (char ch);

static int32_t ascii2bin(char *s)
{
	int32_t radix = 10;
	int32_t result = 0;
	x2bin convertfunc = char2bin;

	switch (*s) {
	case '$':
		radix = 16;
		s += 1;
		convertfunc = char2bin;
		break;
	case '&':
		radix = 8;
		s += 1;
		convertfunc = char2bin;
		break;
	case '`':
		radix = 4;
		s += 1;
		convertfunc = gbgfx2bin;
		break;
	case '%':
		radix = 2;
		s += 1;
		convertfunc = binary2bin;
		break;
	default:
		/* Handle below */
		break;
	}

	if (radix == 4) {
		int32_t c;

		while (*s != '\0') {
			c = convertfunc(*s++);
			result = result * 2 + ((c & 2) << 7) + (c & 1);
		}
	} else {
		while (*s != '\0')
			result = result * radix + convertfunc(*s++);
	}

	return result;
}

uint32_t ParseFixedPoint(char *s, uint32_t size)
{
	uint32_t i = 0, dot = 0;

	while (size && dot != 2) {
		if (s[i] == '.')
			dot += 1;

		if (dot < 2) {
			size -= 1;
			i += 1;
		}
	}

	yyunputbytes(size);

	yylval.nConstValue = (int32_t)(atof(s) * 65536);

	return 1;
}

uint32_t ParseNumber(char *s, uint32_t size)
{
	char dest[256];

	strncpy(dest, s, size);
	dest[size] = 0;
	yylval.nConstValue = ascii2bin(dest);

	return 1;
}

uint32_t ParseSymbol(char *src, uint32_t size)
{
	char dest[MAXSYMLEN + 1];
	int32_t copied = 0, size_backup = size;

	while (size && copied < MAXSYMLEN) {
		if (*src == '\\') {
			char *marg;

			src += 1;
			size -= 1;

			if (*src == '@') {
				marg = sym_FindMacroArg(-1);
			} else if (*src >= '0' && *src <= '9') {
				marg = sym_FindMacroArg(*src - '0');
			} else {
				fatalerror("Malformed ID");
				return 0;
			}

			src += 1;
			size -= 1;

			if (marg) {
				while (*marg)
					dest[copied++] = *marg++;
			}
		} else {
			dest[copied++] = *src++;
			size -= 1;
		}
	}

	if (copied > MAXSYMLEN)
		fatalerror("Symbol too long");

	dest[copied] = 0;

	if (!oDontExpandStrings && sym_isString(dest)) {
		char *s;

		yyskipbytes(size_backup);
		yyunputstr(s = sym_GetStringValue(dest));

		while (*s) {
			if (*s++ == '\n')
				nLineNo -= 1;
		}
		return 0;
	}

	strcpy(yylval.tzString, dest);
	return 1;
}

uint32_t PutMacroArg(char *src, uint32_t size)
{
	char *s;

	yyskipbytes(size);
	if ((size == 2 && src[1] >= '1' && src[1] <= '9')) {
		s = sym_FindMacroArg(src[1] - '0');

		if (s != NULL)
			yyunputstr(s);
		else
			yyerror("Macro argument not defined");
	} else {
		yyerror("Invalid macro argument");
	}
	return 0;
}

uint32_t PutUniqueArg(unused_ char *src, uint32_t size)
{
	char *s;

	yyskipbytes(size);

	s = sym_FindMacroArg(-1);

	if (s != NULL)
		yyunputstr(s);
	else
		yyerror("Macro unique label string not defined");

	return 0;
}

enum {
	T_LEX_MACROARG = 3000,
	T_LEX_MACROUNIQUE
};

const struct sLexInitString lexer_strings[] = {
	{"adc", T_Z80_ADC},
	{"add", T_Z80_ADD},
	{"and", T_Z80_AND},
	{"bit", T_Z80_BIT},
	{"call", T_Z80_CALL},
	{"ccf", T_Z80_CCF},
	{"cpl", T_Z80_CPL},
	{"cp", T_Z80_CP},
	{"daa", T_Z80_DAA},
	{"dec", T_Z80_DEC},
	{"di", T_Z80_DI},
	{"ei", T_Z80_EI},
	{"halt", T_Z80_HALT},
	{"inc", T_Z80_INC},
	{"jp", T_Z80_JP},
	{"jr", T_Z80_JR},
	{"ld", T_Z80_LD},
	{"ldi", T_Z80_LDI},
	{"ldd", T_Z80_LDD},
	{"ldio", T_Z80_LDIO},
	{"ldh", T_Z80_LDIO},
	{"nop", T_Z80_NOP},
	{"or", T_Z80_OR},
	{"pop", T_Z80_POP},
	{"push", T_Z80_PUSH},
	{"res", T_Z80_RES},
	{"reti", T_Z80_RETI},
	{"ret", T_Z80_RET},
	{"rlca", T_Z80_RLCA},
	{"rlc", T_Z80_RLC},
	{"rla", T_Z80_RLA},
	{"rl", T_Z80_RL},
	{"rrc", T_Z80_RRC},
	{"rrca", T_Z80_RRCA},
	{"rra", T_Z80_RRA},
	{"rr", T_Z80_RR},
	{"rst", T_Z80_RST},
	{"sbc", T_Z80_SBC},
	{"scf", T_Z80_SCF},
	{"set", T_POP_SET},
	{"sla", T_Z80_SLA},
	{"sra", T_Z80_SRA},
	{"srl", T_Z80_SRL},
	{"stop", T_Z80_STOP},
	{"sub", T_Z80_SUB},
	{"swap", T_Z80_SWAP},
	{"xor", T_Z80_XOR},

	{"nz", T_CC_NZ},
	{"z", T_CC_Z},
	{"nc", T_CC_NC},
	/* Handled in list of registers */
	/* { "c", T_TOKEN_C }, */

	{"[bc]", T_MODE_BC_IND},
	{"[de]", T_MODE_DE_IND},
	{"[hl]", T_MODE_HL_IND},
	{"[hl+]", T_MODE_HL_INDINC},
	{"[hl-]", T_MODE_HL_INDDEC},
	{"[hli]", T_MODE_HL_INDINC},
	{"[hld]", T_MODE_HL_INDDEC},
	{"[sp]", T_MODE_SP_IND},
	{"af", T_MODE_AF},
	{"bc", T_MODE_BC},
	{"de", T_MODE_DE},
	{"hl", T_MODE_HL},
	{"sp", T_MODE_SP},
	{"[c]", T_MODE_C_IND},
	{"[$ff00+c]", T_MODE_C_IND},
	{"[$ff00 + c]", T_MODE_C_IND},

	{"a", T_TOKEN_A},
	{"b", T_TOKEN_B},
	{"c", T_TOKEN_C},
	{"d", T_TOKEN_D},
	{"e", T_TOKEN_E},
	{"h", T_TOKEN_H},
	{"l", T_TOKEN_L},

	{"||", T_OP_LOGICOR},
	{"&&", T_OP_LOGICAND},
	{"==", T_OP_LOGICEQU},
	{">", T_OP_LOGICGT},
	{"<", T_OP_LOGICLT},
	{">=", T_OP_LOGICGE},
	{"<=", T_OP_LOGICLE},
	{"!=", T_OP_LOGICNE},
	{"!", T_OP_LOGICNOT},
	{"|", T_OP_OR},
	{"^", T_OP_XOR},
	{"&", T_OP_AND},
	{"<<", T_OP_SHL},
	{">>", T_OP_SHR},
	{"+", T_OP_ADD},
	{"-", T_OP_SUB},
	{"*", T_OP_MUL},
	{"/", T_OP_DIV},
	{"%", T_OP_MOD},
	{"~", T_OP_NOT},

	{"def", T_OP_DEF},

	{"bank", T_OP_BANK},
	{"align", T_OP_ALIGN},

	{"round", T_OP_ROUND},
	{"ceil", T_OP_CEIL},
	{"floor", T_OP_FLOOR},
	{"div", T_OP_FDIV},
	{"mul", T_OP_FMUL},
	{"sin", T_OP_SIN},
	{"cos", T_OP_COS},
	{"tan", T_OP_TAN},
	{"asin", T_OP_ASIN},
	{"acos", T_OP_ACOS},
	{"atan", T_OP_ATAN},
	{"atan2", T_OP_ATAN2},

	{"high", T_OP_HIGH},
	{"low", T_OP_LOW},

	{"strcmp", T_OP_STRCMP},
	{"strin", T_OP_STRIN},
	{"strsub", T_OP_STRSUB},
	{"strlen", T_OP_STRLEN},
	{"strcat", T_OP_STRCAT},
	{"strupr", T_OP_STRUPR},
	{"strlwr", T_OP_STRLWR},

	{"include", T_POP_INCLUDE},
	{"printt", T_POP_PRINTT},
	{"printi", T_POP_PRINTI},
	{"printv", T_POP_PRINTV},
	{"printf", T_POP_PRINTF},
	{"export", T_POP_EXPORT},
	{"xdef", T_POP_EXPORT},
	{"import", T_POP_IMPORT},
	{"xref", T_POP_IMPORT},
	{"global", T_POP_GLOBAL},
	{"ds", T_POP_DS},
	{"db", T_POP_DB},
	{"dw", T_POP_DW},
	{"dl", T_POP_DL},
	{"section", T_POP_SECTION},
	{"purge", T_POP_PURGE},

	{"rsreset", T_POP_RSRESET},
	{"rsset", T_POP_RSSET},

	{"incbin", T_POP_INCBIN},
	{"charmap", T_POP_CHARMAP},

	{"fail", T_POP_FAIL},
	{"warn", T_POP_WARN},

	{"macro", T_POP_MACRO},
	/* Not needed but we have it here just to protect the name */
	{"endm", T_POP_ENDM},
	{"shift", T_POP_SHIFT},

	{"rept", T_POP_REPT},
	/* Not needed but we have it here just to protect the name */
	{"endr", T_POP_ENDR},

	{"if", T_POP_IF},
	{"else", T_POP_ELSE},
	{"elif", T_POP_ELIF},
	{"endc", T_POP_ENDC},

	{"union", T_POP_UNION},
	{"nextu", T_POP_NEXTU},
	{"endu", T_POP_ENDU},

	{"wram0", T_SECT_WRAM0},
	{"vram", T_SECT_VRAM},
	{"romx", T_SECT_ROMX},
	{"rom0", T_SECT_ROM0},
	{"hram", T_SECT_HRAM},
	{"wramx", T_SECT_WRAMX},
	{"sram", T_SECT_SRAM},
	{"oam", T_SECT_OAM},

	/* Deprecated section type names */
	{"home", T_SECT_HOME},
	{"code", T_SECT_CODE},
	{"data", T_SECT_DATA},
	{"bss", T_SECT_BSS},

	{"rb", T_POP_RB},
	{"rw", T_POP_RW},
	{"equ", T_POP_EQU},
	{"equs", T_POP_EQUS},

	/*  Handled before in list of CPU instructions */
	/* {"set", T_POP_SET}, */
	{"=", T_POP_SET},

	{"pushs", T_POP_PUSHS},
	{"pops", T_POP_POPS},
	{"pusho", T_POP_PUSHO},
	{"popo", T_POP_POPO},

	{"opt", T_POP_OPT},

	{NULL, 0}
};

const struct sLexFloat tNumberToken = {
	ParseNumber,
	T_NUMBER
};

const struct sLexFloat tFixedPointToken = {
	ParseFixedPoint,
	T_NUMBER
};

const struct sLexFloat tIDToken = {
	ParseSymbol,
	T_ID
};

const struct sLexFloat tMacroArgToken = {
	PutMacroArg,
	T_LEX_MACROARG
};

const struct sLexFloat tMacroUniqueToken = {
	PutUniqueArg,
	T_LEX_MACROUNIQUE
};

void setup_lexer(void)
{
	uint32_t id;

	lex_Init();
	lex_AddStrings(lexer_strings);

	//Macro arguments

	id = lex_FloatAlloc(&tMacroArgToken);
	lex_FloatAddFirstRange(id, '\\', '\\');
	lex_FloatAddSecondRange(id, '1', '9');
	id = lex_FloatAlloc(&tMacroUniqueToken);
	lex_FloatAddFirstRange(id, '\\', '\\');
	lex_FloatAddSecondRange(id, '@', '@');

	//Decimal constants

	id = lex_FloatAlloc(&tNumberToken);
	lex_FloatAddFirstRange(id, '0', '9');
	lex_FloatAddSecondRange(id, '0', '9');
	lex_FloatAddRange(id, '0', '9');

	//Binary constants

	id = lex_FloatAlloc(&tNumberToken);
	nBinaryID = id;
	lex_FloatAddFirstRange(id, '%', '%');
	lex_FloatAddSecondRange(id, CurrentOptions.binary[0],
				CurrentOptions.binary[0]);
	lex_FloatAddSecondRange(id, CurrentOptions.binary[1],
				CurrentOptions.binary[1]);
	lex_FloatAddRange(id, CurrentOptions.binary[0],
			  CurrentOptions.binary[0]);
	lex_FloatAddRange(id, CurrentOptions.binary[1],
			  CurrentOptions.binary[1]);

	//Octal constants

	id = lex_FloatAlloc(&tNumberToken);
	lex_FloatAddFirstRange(id, '&', '&');
	lex_FloatAddSecondRange(id, '0', '7');
	lex_FloatAddRange(id, '0', '7');

	//Gameboy gfx constants

	id = lex_FloatAlloc(&tNumberToken);
	nGBGfxID = id;
	lex_FloatAddFirstRange(id, '`', '`');
	lex_FloatAddSecondRange(id, CurrentOptions.gbgfx[0],
				CurrentOptions.gbgfx[0]);
	lex_FloatAddSecondRange(id, CurrentOptions.gbgfx[1],
				CurrentOptions.gbgfx[1]);
	lex_FloatAddSecondRange(id, CurrentOptions.gbgfx[2],
				CurrentOptions.gbgfx[2]);
	lex_FloatAddSecondRange(id, CurrentOptions.gbgfx[3],
				CurrentOptions.gbgfx[3]);
	lex_FloatAddRange(id, CurrentOptions.gbgfx[0], CurrentOptions.gbgfx[0]);
	lex_FloatAddRange(id, CurrentOptions.gbgfx[1], CurrentOptions.gbgfx[1]);
	lex_FloatAddRange(id, CurrentOptions.gbgfx[2], CurrentOptions.gbgfx[2]);
	lex_FloatAddRange(id, CurrentOptions.gbgfx[3], CurrentOptions.gbgfx[3]);

	//Hex constants

	id = lex_FloatAlloc(&tNumberToken);
	lex_FloatAddFirstRange(id, '$', '$');
	lex_FloatAddSecondRange(id, '0', '9');
	lex_FloatAddSecondRange(id, 'A', 'F');
	lex_FloatAddSecondRange(id, 'a', 'f');
	lex_FloatAddRange(id, '0', '9');
	lex_FloatAddRange(id, 'A', 'F');
	lex_FloatAddRange(id, 'a', 'f');

	//ID 's

	id = lex_FloatAlloc(&tIDToken);
	lex_FloatAddFirstRange(id, 'a', 'z');
	lex_FloatAddFirstRange(id, 'A', 'Z');
	lex_FloatAddFirstRange(id, '_', '_');
	lex_FloatAddSecondRange(id, 'a', 'z');
	lex_FloatAddSecondRange(id, 'A', 'Z');
	lex_FloatAddSecondRange(id, '0', '9');
	lex_FloatAddSecondRange(id, '_', '_');
	lex_FloatAddSecondRange(id, '\\', '\\');
	lex_FloatAddSecondRange(id, '@', '@');
	lex_FloatAddSecondRange(id, '#', '#');
	lex_FloatAddRange(id, '.', '.');
	lex_FloatAddRange(id, 'a', 'z');
	lex_FloatAddRange(id, 'A', 'Z');
	lex_FloatAddRange(id, '0', '9');
	lex_FloatAddRange(id, '_', '_');
	lex_FloatAddRange(id, '\\', '\\');
	lex_FloatAddRange(id, '@', '@');
	lex_FloatAddRange(id, '#', '#');

	//Local ID

	id = lex_FloatAlloc(&tIDToken);
	lex_FloatAddFirstRange(id, '.', '.');
	lex_FloatAddSecondRange(id, 'a', 'z');
	lex_FloatAddSecondRange(id, 'A', 'Z');
	lex_FloatAddSecondRange(id, '_', '_');
	lex_FloatAddRange(id, 'a', 'z');
	lex_FloatAddRange(id, 'A', 'Z');
	lex_FloatAddRange(id, '0', '9');
	lex_FloatAddRange(id, '_', '_');
	lex_FloatAddRange(id, '\\', '\\');
	lex_FloatAddRange(id, '@', '@');
	lex_FloatAddRange(id, '#', '#');

	// "@"

	id = lex_FloatAlloc(&tIDToken);
	lex_FloatAddFirstRange(id, '@', '@');

	//Fixed point constants

	id = lex_FloatAlloc(&tFixedPointToken);
	lex_FloatAddFirstRange(id, '.', '.');
	lex_FloatAddFirstRange(id, '0', '9');
	lex_FloatAddSecondRange(id, '.', '.');
	lex_FloatAddSecondRange(id, '0', '9');
	lex_FloatAddRange(id, '.', '.');
	lex_FloatAddRange(id, '0', '9');
}
