#include "asm/asm.h"
#include "asm/symbol.h"
#include "asm/rpn.h"
#include "asm/symbol.h"
#include "asm/main.h"
#include "asm/lexer.h"

#include "asmy.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

bool oDontExpandStrings = false;
SLONG nGBGfxID = -1;
SLONG nBinaryID = -1;

SLONG 
gbgfx2bin(char ch)
{
	SLONG i;

	for (i = 0; i <= 3; i += 1) {
		if (CurrentOptions.gbgfx[i] == ch) {
			return (i);
		}
	}

	return (0);
}

SLONG 
binary2bin(char ch)
{
	SLONG i;

	for (i = 0; i <= 1; i += 1) {
		if (CurrentOptions.binary[i] == ch) {
			return (i);
		}
	}

	return (0);
}

SLONG 
char2bin(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);

	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A' + 10);

	if (ch >= '0' && ch <= '9')
		return (ch - '0');

	return (0);
}

typedef SLONG(*x2bin) (char ch);

SLONG 
ascii2bin(char *s)
{
	SLONG radix = 10;
	SLONG result = 0;
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
	}

	if (radix == 4) {
		SLONG c;

		while (*s != '\0') {
			c = convertfunc(*s++);
			result = result * 2 + ((c & 2) << 7) + (c & 1);
		}
	} else {
		while (*s != '\0')
			result = result * radix + convertfunc(*s++);
	}

	return (result);
}

ULONG 
ParseFixedPoint(char *s, ULONG size)
{
	//char dest[256];
	ULONG i = 0, dot = 0;

	while (size && dot != 2) {
		if (s[i] == '.')
			dot += 1;

		if (dot < 2) {
			//dest[i] = s[i];
			size -= 1;
			i += 1;
		}
	}

	//dest[i] = 0;

	yyunputbytes(size);

	yylval.nConstValue = (SLONG) (atof(s) * 65536);

	return (1);
}

ULONG 
ParseNumber(char *s, ULONG size)
{
	char dest[256];

	strncpy(dest, s, size);
	dest[size] = 0;
	yylval.nConstValue = ascii2bin(dest);

	return (1);
}

ULONG 
ParseSymbol(char *src, ULONG size)
{
	char dest[MAXSYMLEN + 1];
	int copied = 0, size_backup = size;

	while (size && copied < MAXSYMLEN) {
		if (*src == '\\') {
			char *marg;

			src += 1;
			size -= 1;

			if (*src == '@')
				marg = sym_FindMacroArg(-1);
			else if (*src >= '0' && *src <= '9')
				marg = sym_FindMacroArg(*src - '0');
			else {
				fatalerror("Malformed ID");
				return (0);
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
			if (*s++ == '\n') {
				nLineNo -= 1;
			}
		}
		return (0);
	} else {
		strcpy(yylval.tzString, dest);
		return (1);
	}
}

ULONG 
PutMacroArg(char *src, ULONG size)
{
	char *s;

	yyskipbytes(size);
	if ((size == 2 && src[1] >= '1' && src[1] <= '9')) {
		if ((s = sym_FindMacroArg(src[1] - '0')) != NULL) {
			yyunputstr(s);
		} else {
			yyerror("Macro argument not defined");
		}
	} else {
		yyerror("Invalid macro argument");
	}
	return (0);
}

ULONG 
PutUniqueArg(char *src, ULONG size)
{
	char *s;

	yyskipbytes(size);
	if ((s = sym_FindMacroArg(-1)) != NULL) {
		yyunputstr(s);
	} else {
		yyerror("Macro unique label string not defined");
	}
	return (0);
}

enum {
	T_LEX_MACROARG = 3000,
	T_LEX_MACROUNIQUE
};

extern struct sLexInitString localstrings[];

struct sLexInitString staticstrings[] = {
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

	{"strcmp", T_OP_STRCMP},
	{"strin", T_OP_STRIN},
	{"strsub", T_OP_STRSUB},
	{"strlen", T_OP_STRLEN},
	{"strcat", T_OP_STRCAT},
	{"strupr", T_OP_STRUPR},
	{"strlwr", T_OP_STRLWR},

	{"include", T_POP_INCLUDE},
	{"printt", T_POP_PRINTT},
	{"printv", T_POP_PRINTV},
	{"printf", T_POP_PRINTF},
	{"export", T_POP_EXPORT},
	{"xdef", T_POP_EXPORT},
	{"import", T_POP_IMPORT},
	{"xref", T_POP_IMPORT},
	{"global", T_POP_GLOBAL},
	{"ds", T_POP_DS},
	{NAME_DB, T_POP_DB},
	{NAME_DW, T_POP_DW},
#ifdef NAME_DL
	{NAME_DL, T_POP_DL},
#endif
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
	{"endc", T_POP_ENDC},

	{"wram0", T_SECT_WRAM0},
	{"bss", T_SECT_WRAM0}, /* deprecated */
	{"vram", T_SECT_VRAM},
	{"code", T_SECT_ROMX}, /* deprecated */
	{"data", T_SECT_ROMX}, /* deprecated */
	{"romx", T_SECT_ROMX},
	{"home", T_SECT_ROM0}, /* deprecated */
	{"rom0", T_SECT_ROM0},
	{"hram", T_SECT_HRAM},
	{"wramx", T_SECT_WRAMX},
	{"sram", T_SECT_SRAM},
	{"oam", T_SECT_OAM},

	{NAME_RB, T_POP_RB},
	{NAME_RW, T_POP_RW},
#ifdef NAME_RL
	{NAME_RL, T_POP_RL},
#endif
	{"equ", T_POP_EQU},
	{"equs", T_POP_EQUS},

	{"set", T_POP_SET},
	{"=", T_POP_SET},

	{"pushs", T_POP_PUSHS},
	{"pops", T_POP_POPS},
	{"pusho", T_POP_PUSHO},
	{"popo", T_POP_POPO},

	{"opt", T_POP_OPT},

	{NULL, 0}
};

struct sLexFloat tNumberToken = {
	ParseNumber,
	T_NUMBER
};

struct sLexFloat tFixedPointToken = {
	ParseFixedPoint,
	T_NUMBER
};

struct sLexFloat tIDToken = {
	ParseSymbol,
	T_ID
};

struct sLexFloat tMacroArgToken = {
	PutMacroArg,
	T_LEX_MACROARG
};

struct sLexFloat tMacroUniqueToken = {
	PutUniqueArg,
	T_LEX_MACROUNIQUE
};

void 
setuplex(void)
{
	ULONG id;

	lex_Init();
	lex_AddStrings(staticstrings);
	lex_AddStrings(localstrings);

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

	    nBinaryID = id = lex_FloatAlloc(&tNumberToken);
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

	    nGBGfxID = id = lex_FloatAlloc(&tNumberToken);
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

	//@ID

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
