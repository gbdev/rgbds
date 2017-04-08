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
%option yylineno

%{
#include <stdarg.h>
#include <unistd.h>

#include "extern/err.h"
#include "link/mylink.h"
#include "link/script.h"

#include "parser.h"

/* File include stack. */

#define	MAX_INCLUDE_DEPTH 8

static int include_stack_ptr = 0;

static YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
static char include_path[MAX_INCLUDE_DEPTH][_MAX_PATH + 1];
static int include_line[MAX_INCLUDE_DEPTH];

static char linkerscript_path[_MAX_PATH + 1]; /* Base file */
%}

%%

\"([^\\\"]|\\.)*\" {
			if (strlen(yytext) > sizeof(yylval.s) - 1)
				script_fatalerror("String is too long: %s\n.", yytext);
			if (strlen(yytext) < 3) /* 2 quotes + 1 character */
				script_fatalerror("String %s is invalid\n.", yytext);

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

(?i:INCLUDE)	{ return COMMAND_INCLUDE; }

"\n"		{ return NEWLINE; }

;.*		{ /* Ignore comments. A dot doesn't match newline. */ }

[[:space:]]	{ /* Ignore whitespace. */ }

.		{ script_fatalerror("Invalid character [%s]\n.", yytext); }

%%

extern FILE *yyin;

void script_Parse(const char * path)
{
	yyin = fopen(path, "r");

	if (!yyin)
		errx(1, "Error opening file! \"%s\"\n", path);

	strncpy(linkerscript_path, path, sizeof(linkerscript_path));
	linkerscript_path[sizeof(linkerscript_path) - 1] = '\0';

	do {
		yyparse();
	} while (!feof(yyin));

	fclose(yyin);

}

void script_IncludeFile(const char * path)
{
	if (include_stack_ptr == (MAX_INCLUDE_DEPTH-1))
		script_fatalerror("Includes nested too deeply.");

	include_line[include_stack_ptr] = yylineno;
	include_stack[include_stack_ptr] = YY_CURRENT_BUFFER;

	include_stack_ptr++;

	yyin = fopen(path, "r" );

	if (!yyin)
		script_fatalerror("Couldn't open file \"%s\"", path);

	strncpy(include_path[include_stack_ptr], path, sizeof(include_path[0]));
	include_path[include_stack_ptr][sizeof(include_path[0])-1] = '\0';

	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
	yylineno = 0;
}

int script_IncludeDepthGet(void)
{
	return include_stack_ptr;
}

void script_IncludePop(void)
{
	fclose(yyin);

	include_stack_ptr--;

	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(include_stack[include_stack_ptr]);
	yylineno = include_line[include_stack_ptr];
}

void script_PrintFileStack(void)
{
	int i = include_stack_ptr;

	include_line[i] = yylineno;

	while (i > 0) {
		fprintf(stderr, "%s(%d) -> ", include_path[i], include_line[i]);
		i--;
	}
	fprintf(stderr, "%s(%d)", linkerscript_path, include_line[i]);
}

noreturn void script_fatalerror(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	script_PrintFileStack();
	fprintf(stderr, ":\n\t");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

