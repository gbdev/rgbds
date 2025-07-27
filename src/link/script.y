// SPDX-License-Identifier: MIT

%language "c++"
%define api.value.type variant
%define api.token.constructor

%code requires {
	#include <stdint.h>
	#include <string>

	#include "linkdefs.hpp"
}

%code {
	#include "link/lexer.hpp"
	#include "link/layout.hpp"

	yy::parser::symbol_type yylex(); // Provided by layout.cpp
}

/******************** Tokens and data types ********************/

%token YYEOF 0 "end of file"
%token newline "end of line"

%token COMMA ","

// Keywords
%token ORG "ORG"
%token FLOATING "FLOATING"
%token INCLUDE "INCLUDE"
%token ALIGN "ALIGN"
%token DS "DS"
%token OPTIONAL "OPTIONAL"

// Literals
%token <std::string> string;
%token <uint32_t> number;
%token <SectionType> sect_type;

%type <bool> optional;

%%

/******************** Parser rules ********************/

lines:
	  %empty
	| line lines
;

line:
	INCLUDE string newline {
		lexer_IncludeFile(std::move($2)); // Note: this additionally increments the line number!
	}
	| directive newline {
		lexer_IncLineNo();
	}
	| newline {
		lexer_IncLineNo();
	}
	// Error recovery.
	| error newline {
		yyerrok;
		lexer_IncLineNo();
	}
;

directive:
	sect_type {
		layout_SetSectionType($1);
	}
	| sect_type number {
		layout_SetSectionType($1, $2);
	}
	| sect_type FLOATING {
		layout_SetFloatingSectionType($1);
	}
	| FLOATING {
		layout_MakeAddrFloating();
	}
	| ORG number {
		layout_SetAddr($2);
	}
	| ALIGN number {
		layout_AlignTo($2, 0);
	}
	| ALIGN number COMMA number {
		layout_AlignTo($2, $4);
	}
	| DS number {
		layout_Pad($2);
	}
	| string optional {
		layout_PlaceSection($1, $2);
	}
;

optional:
	%empty {
		$$ = false;
	}
	| OPTIONAL {
		$$ = true;
	}
;

%%

/******************** Error handler ********************/

void yy::parser::error(std::string const &msg) {
	lexer_Error("%s", msg.c_str());
}
