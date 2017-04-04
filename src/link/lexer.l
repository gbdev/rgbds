/*
 * Copyright (C) 2017 Antonio Nino Diaz <antonio_nd@outlook.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

%option noinput
%option nounput

%{
#include <unistd.h>

#include "extern/err.h"

#include "parser.h"
%}

%%

\"([^\\\"]|\\.)*\" {
			if (strlen(yytext) > sizeof(yylval.s) - 1)
				errx(1, "String is too long: \"%s\"\n.", yytext);
			if (strlen(yytext) < 3) /* 2 quotes + 1 character */
				errx(1, "String \"%s\" is invalid\n.", yytext);

			yytext++; /* ignore first quote */
			strcpy(yylval.s, yytext);
			yylval.s[strlen(yylval.s)-1] = '\0'; /* remove end quote */

			return STRING;
		}

\$[a-fA-F0-9]+	{
			yytext++; /* Skip prefix */
			yylval.i = strtol(yytext, NULL, 16);
			return INTEGER;
		}
[0-9]+		{
			yylval.i = strtol(yytext, NULL, 10);
			return INTEGER;
		}

(?i:ROM0)	{ strcpy(yylval.s, "ROM0");  return SECTION_NONBANKED; }
(?i:ROMX)	{ strcpy(yylval.s, "ROMX");  return SECTION_BANKED; }
(?i:VRAM)	{ strcpy(yylval.s, "VRAM");  return SECTION_BANKED; }
(?i:WRAM0)	{ strcpy(yylval.s, "WRAM0"); return SECTION_NONBANKED; }
(?i:WRAMX)	{ strcpy(yylval.s, "WRAMX"); return SECTION_BANKED; }
(?i:SRAM)	{ strcpy(yylval.s, "SRAM");  return SECTION_BANKED; }
(?i:OAM)	{ strcpy(yylval.s, "OAM");   return SECTION_NONBANKED; }
(?i:HRAM)	{ strcpy(yylval.s, "HRAM");  return SECTION_NONBANKED; }

(?i:ALIGN)	{ return COMMAND_ALIGN; }
(?i:ORG)	{ return COMMAND_ORG; }

"\n"		{ return NEWLINE; }

;.*		{ /* Ignore comments. A dot doesn't match newline. */ }

[[:space:]]	{ /* Ignore whitespace. */ }

.		{ errx(1, "Invalid character [%s]\n.", yytext); }

%%

