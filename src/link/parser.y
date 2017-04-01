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

%{
#include <stdio.h>

#include "extern/err.h"
#include "link/script.h"

int yylex();
void yyerror(char *);

static int nline = 1;
%}

%union { int i; char s[512]; }

%token<i> INTEGER
%token<s> STRING

%token<s> SECTION_NONBANKED
%token<s> SECTION_BANKED

%token COMMAND_ALIGN
%token COMMAND_ORG

%token NEWLINE

%start lines

%%

lines:
      /* empty */
    | lines line NEWLINE
    ;

line:
      /* empty */       { nline++; }
    | statement         { nline++; }
    ;

statement:
    /* Statements to set the current section */
      SECTION_NONBANKED {
        script_SetCurrentSectionType($1, 0);
    }
    | SECTION_NONBANKED INTEGER {
        errx(1, "%d:Trying to assign a bank to a non-banked section.\n", nline);
    }

    | SECTION_BANKED {
        errx(1, "%d:Banked section without assigned bank.\n", nline);
    }
    | SECTION_BANKED INTEGER {
        script_SetCurrentSectionType($1, $2);
    }

    /* Commands to adjust the address inside the current section */
    | COMMAND_ALIGN INTEGER {
        script_SetAlignment($2);
    }
    | COMMAND_ALIGN {
        errx(1, "%d:ALIGN keyword needs an argument.\n", nline);
    }
    | COMMAND_ORG INTEGER {
        script_SetAddress($2);
    }
    | COMMAND_ORG {
        errx(1, "%d:ORG keyword needs an argument.\n", nline);
    }

    /* Section name */
    | STRING {
        script_OutputSection($1);
    }

    /* End */
    ;

%%

extern int yylex();
extern int yyparse();

extern FILE *yyin;

void yyerror(char *s)
{
    errx(1, "%d:Linkerscript parse error: \"%s\"\n", nline, s);
}

void script_Parse(const char *path)
{
    script_InitSections();

    FILE *f = fopen(path, "r");

    if (!f)
        errx(1, "Error opening file! \"%s\"\n", path);

    yyin = f;

    do {
        yyparse();
    } while (!feof(yyin));

    fclose(f);
}

