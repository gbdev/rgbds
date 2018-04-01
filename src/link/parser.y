/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2017-2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

%{
#include <stdint.h>
#include <stdio.h>

#include "extern/err.h"

#include "link/script.h"

int yylex(void);
void yyerror(char *);

extern int yylineno;
%}

%union {
	int32_t i;
	char s[512];
}

%token<i> INTEGER
%token<s> STRING

%token<s> SECTION_NONBANKED
%token<s> SECTION_BANKED

%token COMMAND_ALIGN
%token COMMAND_ORG

%token COMMAND_INCLUDE

%token NEWLINE

%start lines

%%

lines:
	  /* empty */
	| lines line NEWLINE
;

line:
	  /* empty */
	| statement
;

statement:
	/* Statements to set the current section */
	  SECTION_NONBANKED
	{
		script_SetCurrentSectionType($1, 0);
	}
	| SECTION_NONBANKED INTEGER
	{
		script_fatalerror("Trying to assign a bank to a non-banked section.\n");
	}

	| SECTION_BANKED
	{
		script_fatalerror("Banked section without assigned bank.\n");
	}
	| SECTION_BANKED INTEGER
	{
		script_SetCurrentSectionType($1, $2);
	}

	/* Commands to adjust the address inside the current section */
	| COMMAND_ALIGN INTEGER
	{
		script_SetAlignment($2);
	}
	| COMMAND_ALIGN
	{
		script_fatalerror("ALIGN keyword needs an argument.\n");
	}
	| COMMAND_ORG INTEGER
	{
		script_SetAddress($2);
	}
	| COMMAND_ORG
	{
		script_fatalerror("ORG keyword needs an argument.\n");
	}

	/* Section name */
	| STRING
	{
		script_OutputSection($1);
	}

	/* Include file */
	| COMMAND_INCLUDE STRING
	{
		script_IncludeFile($2);
	}

	/* End */
;

%%

extern int yylex(void);
extern int yyparse(void);

int yywrap(void)
{
	if (script_IncludeDepthGet() == 0)
		return 1;

	script_IncludePop();

	return 0;
}

void yyerror(char *s)
{
	script_fatalerror("Linkerscript parse error: \"%s\"\n", s);
}

