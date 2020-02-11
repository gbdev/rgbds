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
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/warning.h"

#include "helpers.h"

#include "asmy.h"

bool oDontExpandStrings;
int32_t nGBGfxID = -1;
int32_t nBinaryID = -1;

static int32_t gbgfx2bin(char ch)
{
	int32_t i;

	for (i = 0; i <= 3; i++) {
		if (CurrentOptions.gbgfx[i] == ch)
			return i;
	}

	return 0;
}

static int32_t binary2bin(char ch)
{
	int32_t i;

	for (i = 0; i <= 1; i++) {
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
	char *start = s;
	uint32_t radix = 10;
	uint32_t result = 0;
	x2bin convertfunc = char2bin;

	switch (*s) {
	case '$':
		radix = 16;
		s++;
		convertfunc = char2bin;
		break;
	case '&':
		radix = 8;
		s++;
		convertfunc = char2bin;
		break;
	case '`':
		radix = 4;
		s++;
		convertfunc = gbgfx2bin;
		break;
	case '%':
		radix = 2;
		s++;
		convertfunc = binary2bin;
		break;
	default:
		/* Handle below */
		break;
	}

	const uint32_t max_q = UINT32_MAX / radix;
	const uint32_t max_r = UINT32_MAX % radix;

	if (*s == '\0') {
		/*
		 * There are no digits after the radix prefix
		 * (or the string is empty, which shouldn't happen).
		 */
		yyerror("Invalid integer constant");
	} else if (radix == 4) {
		int32_t size = 0;
		int32_t c;

		while (*s != '\0') {
			c = convertfunc(*s++);
			result = result * 2 + ((c & 2) << 7) + (c & 1);
			size++;
		}

		/*
		 * Extending a graphics constant longer than 8 pixels,
		 * the Game Boy tile width, produces a nonsensical result.
		 */
		if (size > 8) {
			warning(WARNING_LARGE_CONSTANT, "Graphics constant '%s' is too long",
				start);
		}
	} else {
		bool overflow = false;

		while (*s != '\0') {
			int32_t digit = convertfunc(*s++);

			if (result > max_q
			 || (result == max_q && digit > max_r)) {
				overflow = true;
			}
			result = result * radix + digit;
		}

		if (overflow)
			warning(WARNING_LARGE_CONSTANT, "Integer constant '%s' is too large",
				start);
	}

	return result;
}

uint32_t ParseFixedPoint(char *s, uint32_t size)
{
	uint32_t i;
	uint32_t dot = 0;

	for (i = 0; i < size; i++) {
		if (s[i] == '.') {
			dot++;

			if (dot == 2)
				break;
		}
	}

	yyskipbytes(i);

	yylval.nConstValue = (int32_t)(atof(s) * 65536);

	return 1;
}

uint32_t ParseNumber(char *s, uint32_t size)
{
	char dest[256];

	if (size > 255)
		fatalerror("Number token too long");

	strncpy(dest, s, size);
	dest[size] = 0;
	yylval.nConstValue = ascii2bin(dest);

	yyskipbytes(size);

	return 1;
}

/*
 * If the symbol name ends before the end of the macro arg,
 * return a pointer to the rest of the macro arg.
 * Otherwise, return NULL.
 */
char *AppendMacroArg(char whichArg, char *dest, size_t *destIndex)
{
	char *marg;

	if (whichArg == '@')
		marg = sym_FindMacroArg(-1);
	else if (whichArg >= '1' && whichArg <= '9')
		marg = sym_FindMacroArg(whichArg - '0');
	else
		fatalerror("Invalid macro argument '\\%c' in symbol", whichArg);

	if (!marg)
		fatalerror("Macro argument '\\%c' not defined", whichArg);

	char ch;

	while ((ch = *marg) != 0) {
		if ((ch >= 'a' && ch <= 'z')
		 || (ch >= 'A' && ch <= 'Z')
		 || (ch >= '0' && ch <= '9')
		 || ch == '_'
		 || ch == '@'
		 || ch == '#'
		 || ch == '.') {
			if (*destIndex >= MAXSYMLEN)
				fatalerror("Symbol too long");

			dest[*destIndex] = ch;
			(*destIndex)++;
		} else {
			return marg;
		}

		marg++;
	}

	return NULL;
}

uint32_t ParseSymbol(char *src, uint32_t size)
{
	char dest[MAXSYMLEN + 1];
	size_t srcIndex = 0;
	size_t destIndex = 0;
	char *rest = NULL;

	while (srcIndex < size) {
		char ch = src[srcIndex++];

		if (ch == '\\') {
			/*
			 * We don't check if srcIndex is still less than size,
			 * but that can only fail to be true when the
			 * following char is neither '@' nor a digit.
			 * In that case, AppendMacroArg() will catch the error.
			 */
			ch = src[srcIndex++];

			rest = AppendMacroArg(ch, dest, &destIndex);
			/* If the symbol's end was in the middle of the token */
			if (rest)
				break;
		} else {
			if (destIndex >= MAXSYMLEN)
				fatalerror("Symbol too long");
			dest[destIndex++] = ch;
		}
	}

	dest[destIndex] = 0;

	/* Tell the lexer we read all bytes that we did */
	yyskipbytes(srcIndex);

	/*
	 * If an escape's expansion left some chars after the symbol's end,
	 * such as the `::` in a `Backup\1` expanded to `BackupCamX::`,
	 * put those into the buffer.
	 * Note that this NEEDS to be done after the `yyskipbytes` above.
	 */
	if (rest)
		yyunputstr(rest);

	/* If the symbol is an EQUS, expand it */
	if (!oDontExpandStrings) {
		struct sSymbol const *sym = sym_FindSymbol(dest);

		if (sym && sym->type == SYM_EQUS) {
			char *s;

			lex_BeginStringExpansion(dest);

			/* Feed the symbol's contents into the buffer */
			yyunputstr(s = sym_GetStringValue(sym));

			/* Lines inserted this way shall not increase nLineNo */
			while (*s) {
				if (*s++ == '\n')
					nLineNo--;
			}
			return 0;
		}
	}

	strcpy(yylval.tzSym, dest);
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
			yyerror("Macro argument '\\%c' not defined", src[1]);
	} else {
		yyerror("Invalid macro argument '\\%c'", src[1]);
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
	{"newcharmap", T_POP_NEWCHARMAP},
	{"setcharmap", T_POP_SETCHARMAP},
	{"pushc", T_POP_PUSHC},
	{"popc", T_POP_POPC},

	{"fail", T_POP_FAIL},
	{"warn", T_POP_WARN},

	{"macro", T_POP_MACRO},
	/* Not needed but we have it here just to protect the name */
	{"endm", T_POP_ENDM},
	{"shift", T_POP_SHIFT},

	{"rept", T_POP_REPT},
	/* Not needed but we have it here just to protect the name */
	{"endr", T_POP_ENDR},

	{"load", T_POP_LOAD},
	{"endl", T_POP_ENDL},

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
	{"=", T_POP_EQUAL},

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
