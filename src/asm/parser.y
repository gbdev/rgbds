/* SPDX-License-Identifier: MIT */

%language "c++"
%define api.value.type variant
%define api.token.constructor

%code requires {
	#include <stdint.h>
	#include <inttypes.h>
	#include <string>
	#include <variant>
	#include <vector>

	#include "asm/format.hpp"
	#include "asm/lexer.hpp"
	#include "asm/macro.hpp"
	#include "asm/rpn.hpp"
	#include "asm/symbol.hpp"

	#include "linkdefs.hpp"

	struct AlignmentSpec {
		uint8_t alignment;
		uint16_t alignOfs;
	};

	struct ForArgs {
		int32_t start;
		int32_t stop;
		int32_t step;
	};

	struct StrFmtArgList {
		std::string format;
		std::vector<std::variant<uint32_t, std::string>> args;
	};
}
%code {
	#include <ctype.h>
	#include <errno.h>
	#include <new>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	#include "asm/charmap.hpp"
	#include "asm/fixpoint.hpp"
	#include "asm/fstack.hpp"
	#include "asm/main.hpp"
	#include "asm/opt.hpp"
	#include "asm/output.hpp"
	#include "asm/section.hpp"
	#include "util.hpp"
	#include "asm/warning.hpp"

	#include "extern/utf8decoder.hpp"

	#include "helpers.hpp"

	static CaptureBody captureBody; // Captures a REPT/FOR or MACRO

	yy::parser::symbol_type yylex(); // Provided by lexer.cpp

	static void upperstring(char *dest, char const *src);
	static void lowerstring(char *dest, char const *src);
	static uint32_t str2int2(std::vector<uint8_t> const &s);
	static const char *strrstr(char const *s1, char const *s2);
	static void errorInvalidUTF8Byte(uint8_t byte, char const *functionName);
	static size_t strlenUTF8(char const *s);
	static void strsubUTF8(char *dest, size_t destLen, char const *src, uint32_t pos, uint32_t len);
	static size_t charlenUTF8(char const *str);
	static void charsubUTF8(char *dest, char const *src, uint32_t pos);
	static uint32_t adjustNegativePos(int32_t pos, size_t len, char const *functionName);
	static void strrpl(
	    char *dest, size_t destLen, char const *src, char const *old, char const *rep
	);
	static void strfmt(
	    char *dest, size_t destLen, char const *spec,
	    std::vector<std::variant<uint32_t, std::string>> &args
	);
	static void compoundAssignment(const char *symName, RPNCommand op, int32_t constValue);
	static void failAssert(AssertionType type);
	static void failAssertMsg(AssertionType type, char const *msg);
	void yyerror(char const *str);

	// The CPU encodes instructions in a logical way, so most instructions actually follow patterns.
	// These enums thus help with bit twiddling to compute opcodes
	enum { REG_B = 0, REG_C, REG_D, REG_E, REG_H, REG_L, REG_HL_IND, REG_A };

	enum {
		REG_BC_IND = 0,
		REG_DE_IND,
		REG_HL_INDINC,
		REG_HL_INDDEC,
	};

	enum {
		REG_BC = 0,
		REG_DE = 1,
		REG_HL = 2,
		// LD/INC/ADD/DEC allow SP, PUSH/POP allow AF
		REG_SP = 3,
		REG_AF = 3
	};

	enum { CC_NZ = 0, CC_Z, CC_NC, CC_C };
}

%type <Expression> relocexpr
%type <Expression> relocexpr_no_str
%type <int32_t> const
%type <int32_t> const_no_str
%type <int32_t> const_8bit
%type <int32_t> uconst
%type <int32_t> rs_uconst
%type <int32_t> const_3bit
%type <Expression> reloc_8bit
%type <Expression> reloc_8bit_no_str
%type <Expression> reloc_8bit_offset
%type <Expression> reloc_16bit
%type <Expression> reloc_16bit_no_str
%type <int32_t> sectiontype

%type <String> string
%type <String> strcat_args
%type <StrFmtArgList> strfmt_args
%type <StrFmtArgList> strfmt_va_args

%type <int32_t> sectorg
%type <SectionSpec> sectattrs

%token <int32_t> NUMBER "number"
%token <String> STRING "string"

%token PERIOD "."
%token COMMA ","
%token COLON ":" DOUBLE_COLON "::"
%token LBRACK "[" RBRACK "]"
%token LPAREN "(" RPAREN ")"
%token NEWLINE "newline"

%token OP_LOGICNOT "!"
%token OP_LOGICAND "&&" OP_LOGICOR "||"
%token OP_LOGICGT ">" OP_LOGICLT "<"
%token OP_LOGICGE ">=" OP_LOGICLE "<="
%token OP_LOGICNE "!=" OP_LOGICEQU "=="
%token OP_ADD "+" OP_SUB "-"
%token OP_OR "|" OP_XOR "^" OP_AND "&"
%token OP_SHL "<<" OP_SHR ">>" OP_USHR ">>>"
%token OP_MUL "*" OP_DIV "/" OP_MOD "%"
%token OP_NOT "~"
%left OP_LOGICOR
%left OP_LOGICAND
%left OP_LOGICGT OP_LOGICLT OP_LOGICGE OP_LOGICLE OP_LOGICNE OP_LOGICEQU
%left OP_ADD OP_SUB
%left OP_OR OP_XOR OP_AND
%left OP_SHL OP_SHR OP_USHR
%left OP_MUL OP_DIV OP_MOD

%precedence NEG // negation -- unary minus

%token OP_EXP "**"
%left OP_EXP

%token OP_DEF "DEF"
%token OP_BANK "BANK"
%token OP_ALIGN "ALIGN"
%token OP_SIZEOF "SIZEOF" OP_STARTOF "STARTOF"

%token OP_SIN "SIN" OP_COS "COS" OP_TAN "TAN"
%token OP_ASIN "ASIN" OP_ACOS "ACOS" OP_ATAN "ATAN" OP_ATAN2 "ATAN2"
%token OP_FDIV "FDIV"
%token OP_FMUL "FMUL"
%token OP_FMOD "FMOD"
%token OP_POW "POW"
%token OP_LOG "LOG"
%token OP_ROUND "ROUND"
%token OP_CEIL "CEIL" OP_FLOOR "FLOOR"
%type <int32_t> opt_q_arg

%token OP_HIGH "HIGH" OP_LOW "LOW"
%token OP_ISCONST "ISCONST"

%token OP_STRCMP "STRCMP"
%token OP_STRIN "STRIN" OP_STRRIN "STRRIN"
%token OP_STRSUB "STRSUB"
%token OP_STRLEN "STRLEN"
%token OP_STRCAT "STRCAT"
%token OP_STRUPR "STRUPR" OP_STRLWR "STRLWR"
%token OP_STRRPL "STRRPL"
%token OP_STRFMT "STRFMT"

%token OP_CHARLEN "CHARLEN"
%token OP_CHARSUB "CHARSUB"
%token OP_INCHARMAP "INCHARMAP"

%token <std::string> LABEL "label"
%token <std::string> ID "identifier"
%token <std::string> LOCAL_ID "local identifier"
%token <std::string> ANON "anonymous label"
%type <std::string> def_id
%type <std::string> redef_id
%type <std::string> scoped_id
%type <std::string> scoped_anon_id
%token POP_EQU "EQU"
%token POP_EQUAL "="
%token POP_EQUS "EQUS"

%token POP_ADDEQ "+=" POP_SUBEQ "-="
%token POP_MULEQ "*=" POP_DIVEQ "/=" POP_MODEQ "%="
%token POP_OREQ "|=" POP_XOREQ "^=" POP_ANDEQ "&="
%token POP_SHLEQ "<<=" POP_SHREQ ">>="
%type <RPNCommand> compoundeq

%token POP_INCLUDE "INCLUDE"
%token POP_PRINT "PRINT" POP_PRINTLN "PRINTLN"
%token POP_IF "IF" POP_ELIF "ELIF" POP_ELSE "ELSE" POP_ENDC "ENDC"
%token POP_EXPORT "EXPORT"
%token POP_DB "DB" POP_DS "DS" POP_DW "DW" POP_DL "DL"
%token POP_SECTION "SECTION" POP_FRAGMENT "FRAGMENT"
%token POP_ENDSECTION "ENDSECTION"
%token POP_RB "RB" POP_RW "RW" // There is no POP_RL, only Z80_RL
%token POP_MACRO "MACRO"
%token POP_ENDM "ENDM"
%token POP_RSRESET "RSRESET" POP_RSSET "RSSET"
%token POP_UNION "UNION" POP_NEXTU "NEXTU" POP_ENDU "ENDU"
%token POP_INCBIN "INCBIN" POP_REPT "REPT" POP_FOR "FOR"
%token POP_CHARMAP "CHARMAP"
%token POP_NEWCHARMAP "NEWCHARMAP"
%token POP_SETCHARMAP "SETCHARMAP"
%token POP_PUSHC "PUSHC"
%token POP_POPC "POPC"
%token POP_SHIFT "SHIFT"
%token POP_ENDR "ENDR"
%token POP_BREAK "BREAK"
%token POP_LOAD "LOAD" POP_ENDL "ENDL"
%token POP_FAIL "FAIL"
%token POP_WARN "WARN"
%token POP_FATAL "FATAL"
%token POP_ASSERT "ASSERT" POP_STATIC_ASSERT "STATIC_ASSERT"
%token POP_PURGE "PURGE"
%token POP_REDEF "REDEF"
%token POP_POPS "POPS"
%token POP_PUSHS "PUSHS"
%token POP_POPO "POPO"
%token POP_PUSHO "PUSHO"
%token POP_OPT "OPT"
%token SECT_ROM0 "ROM0" SECT_ROMX "ROMX"
%token SECT_WRAM0 "WRAM0" SECT_WRAMX "WRAMX" SECT_HRAM "HRAM"
%token SECT_VRAM "VRAM" SECT_SRAM "SRAM" SECT_OAM "OAM"

%type <bool> capture_rept
%type <bool> capture_macro

%type <SectionModifier> sectmod
%type <MacroArgs *> macroargs

%type <AlignmentSpec> align_spec

%type <std::vector<Expression>> ds_args
%type <std::vector<std::string>> purge_args
%type <ForArgs> for_args

%token Z80_ADC "adc" Z80_ADD "add" Z80_AND "and"
%token Z80_BIT "bit"
%token Z80_CALL "call" Z80_CCF "ccf" Z80_CP "cp" Z80_CPL "cpl"
%token Z80_DAA "daa" Z80_DEC "dec" Z80_DI "di"
%token Z80_EI "ei"
%token Z80_HALT "halt"
%token Z80_INC "inc"
%token Z80_JP "jp" Z80_JR "jr"
%token Z80_LD "ld"
%token Z80_LDI "ldi"
%token Z80_LDD "ldd"
%token Z80_LDH "ldh"
%token Z80_NOP "nop"
%token Z80_OR "or"
%token Z80_POP "pop" Z80_PUSH "push"
%token Z80_RES "res" Z80_RET "ret" Z80_RETI "reti" Z80_RST "rst"
%token Z80_RL "rl" Z80_RLA "rla" Z80_RLC "rlc" Z80_RLCA "rlca"
%token Z80_RR "rr" Z80_RRA "rra" Z80_RRC "rrc" Z80_RRCA "rrca"
%token Z80_SBC "sbc" Z80_SCF "scf" Z80_SET "set" Z80_STOP "stop"
%token Z80_SLA "sla" Z80_SRA "sra" Z80_SRL "srl" Z80_SUB "sub"
%token Z80_SWAP "swap"
%token Z80_XOR "xor"

%token TOKEN_A "a"
%token TOKEN_B "b" TOKEN_C "c"
%token TOKEN_D "d" TOKEN_E "e"
%token TOKEN_H "h" TOKEN_L "l"
%token MODE_AF "af" MODE_BC "bc" MODE_DE "de" MODE_SP "sp"
%token MODE_HL "hl" MODE_HL_DEC "hld/hl-" MODE_HL_INC "hli/hl+"
%token CC_NZ "nz" CC_Z "z" CC_NC "nc" // There is no CC_C, only TOKEN_C

%type <int32_t> reg_r
%type <int32_t> reg_ss
%type <int32_t> reg_rr
%type <int32_t> reg_tt
%type <int32_t> ccode_expr
%type <int32_t> ccode
%type <Expression> op_a_n
%type <int32_t> op_a_r
%type <Expression> op_mem_ind
%type <AssertionType> assert_type

%token EOB "end of buffer"
%token YYEOF 0 "end of file"
%start asmfile

%%

// Assembly files.

asmfile: lines
;

lines:
	  %empty
	| lines opt_diff_mark line
;

endofline: NEWLINE | EOB
;

opt_diff_mark:
	  %empty // OK
	| OP_ADD {
		::error(
			"syntax error, unexpected + at the beginning of the line (is it a leftover diff mark?)\n"
		);
	}
	| OP_SUB {
		::error(
			"syntax error, unexpected - at the beginning of the line (is it a leftover diff mark?)\n"
		);
	}
;

// Lines and line directives.

line:
	  plain_directive endofline
	| line_directive // Directives that manage newlines themselves
	// Continue parsing the next line on a syntax error
	| error {
		lexer_SetMode(LEXER_NORMAL);
		lexer_ToggleStringExpansion(true);
	} endofline {
		fstk_StopRept();
		yyerrok;
	}
	// Hint about unindented macros parsed as labels
	| LABEL error {
		lexer_SetMode(LEXER_NORMAL);
		lexer_ToggleStringExpansion(true);
	} endofline {
		Symbol *macro = sym_FindExactSymbol($1.c_str());

		if (macro && macro->type == SYM_MACRO)
		fprintf(
			stderr,
			"    To invoke `%s` as a macro it must be indented\n",
			$1.c_str()
		);
		fstk_StopRept();
		yyerrok;
	}
;

// For "logistical" reasons, these directives must manage newlines themselves.
// This is because we need to switch the lexer's mode *after* the newline has been read,
// and to avoid causing some grammar conflicts (token reducing is finicky).
// This is DEFINITELY one of the more FRAGILE parts of the codebase, handle with care.
line_directive:
	  macrodef
	| rept
	| for
	| break
	| include
	| if
	// It's important that all of these require being at line start for `skipIfBlock`
	| elif
	| else
;

if:
	POP_IF const NEWLINE {
		lexer_IncIFDepth();

		if ($2)
			lexer_RunIFBlock();
		else
			lexer_SetMode(LEXER_SKIP_TO_ELIF);
	}
;

elif:
	POP_ELIF const NEWLINE {
		if (lexer_GetIFDepth() == 0)
			fatalerror("Found ELIF outside an IF construct\n");

		if (lexer_RanIFBlock()) {
			if (lexer_ReachedELSEBlock())
				fatalerror("Found ELIF after an ELSE block\n");

			lexer_SetMode(LEXER_SKIP_TO_ENDC);
		} else if ($2) {
			lexer_RunIFBlock();
		} else {
			lexer_SetMode(LEXER_SKIP_TO_ELIF);
		}
	}
;

else:
	POP_ELSE NEWLINE {
		if (lexer_GetIFDepth() == 0)
			fatalerror("Found ELSE outside an IF construct\n");

		if (lexer_RanIFBlock()) {
			if (lexer_ReachedELSEBlock())
				fatalerror("Found ELSE after an ELSE block\n");

			lexer_SetMode(LEXER_SKIP_TO_ENDC);
		} else {
			lexer_RunIFBlock();
			lexer_ReachELSEBlock();
		}
	}
;

// Directives, labels, functions, and values.

plain_directive:
	  label
	| label cpu_commands
	| label macro
	| label directive
	| assignment_directive
;

endc:
	POP_ENDC {
		lexer_DecIFDepth();
	}
;

def_id:
	OP_DEF {
		lexer_ToggleStringExpansion(false);
	} ID {
		lexer_ToggleStringExpansion(true);
		$$ = $3;
	}
;

redef_id:
	POP_REDEF {
		lexer_ToggleStringExpansion(false);
	} ID {
		lexer_ToggleStringExpansion(true);
		$$ = $3;
	}
;

// LABEL covers identifiers followed by a double colon (e.g. `call Function::ret`,
// to be read as `call Function :: ret`). This should not conflict with anything.
scoped_id: ID | LOCAL_ID | LABEL;
scoped_anon_id: scoped_id | ANON;

label:
	  %empty
	| COLON {
		sym_AddAnonLabel();
	}
	| LOCAL_ID {
		sym_AddLocalLabel($1.c_str());
	}
	| LOCAL_ID COLON {
		sym_AddLocalLabel($1.c_str());
	}
	| LABEL COLON {
		sym_AddLabel($1.c_str());
	}
	| LOCAL_ID DOUBLE_COLON {
		sym_AddLocalLabel($1.c_str());
		sym_Export($1.c_str());
	}
	| LABEL DOUBLE_COLON {
		sym_AddLabel($1.c_str());
		sym_Export($1.c_str());
	}
;

macro:
	ID {
		// Parsing 'macroargs' will restore the lexer's normal mode
		lexer_SetMode(LEXER_RAW);
	} macroargs {
		fstk_RunMacro($1.c_str(), *$3);
	}
;

macroargs:
	%empty {
		$$ = new (std::nothrow) MacroArgs();
		if (!$$)
			fatalerror("Failed to allocate memory for macro arguments: %s\n", strerror(errno));
	}
	| macroargs STRING {
		$1->append($2.string);
		$$ = $1;
	}
;

// These commands start with a LABEL.
assignment_directive: equ | assignment | rb | rw | rl | equs;

directive:
	  endc
	| print
	| println
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
	| shift
	| fail
	| warn
	| assert
	| def_equ
	| redef_equ
	| def_set
	| def_rb
	| def_rw
	| def_rl
	| def_equs
	| redef_equs
	| purge
	| pops
	| pushs
	| endsection
	| popo
	| pusho
	| opt
	| align
;

trailing_comma: %empty | COMMA;

compoundeq:
	POP_ADDEQ {
		$$ = RPN_ADD;
	}
	| POP_SUBEQ {
		$$ = RPN_SUB;
	}
	| POP_MULEQ {
		$$ = RPN_MUL;
	}
	| POP_DIVEQ {
		$$ = RPN_DIV;
	}
	| POP_MODEQ {
		$$ = RPN_MOD;
	}
	| POP_XOREQ {
		$$ = RPN_XOR;
	}
	| POP_OREQ {
		$$ = RPN_OR;
	}
	| POP_ANDEQ {
		$$ = RPN_AND;
	}
	| POP_SHLEQ {
		$$ = RPN_SHL;
	}
	| POP_SHREQ {
		$$ = RPN_SHR;
	}
;

equ:
	LABEL POP_EQU const {
		warning(
		    WARNING_OBSOLETE,
		    "`%s EQU` is deprecated; use `DEF %s EQU`\n",
		    $1.c_str(),
		    $1.c_str()
		);
		sym_AddEqu($1.c_str(), $3);
	}
;

assignment:
	LABEL POP_EQUAL const {
		warning(WARNING_OBSOLETE, "`%s =` is deprecated; use `DEF %s =`\n", $1.c_str(), $1.c_str());
		sym_AddVar($1.c_str(), $3);
	}
	| LABEL compoundeq const {
		const char *compoundEqOperator = nullptr;
		switch ($2) {
			case RPN_ADD: compoundEqOperator = "+="; break;
			case RPN_SUB: compoundEqOperator = "-="; break;
			case RPN_MUL: compoundEqOperator = "*="; break;
			case RPN_DIV: compoundEqOperator = "/="; break;
			case RPN_MOD: compoundEqOperator = "%="; break;
			case RPN_XOR: compoundEqOperator = "^="; break;
			case RPN_OR:  compoundEqOperator = "|="; break;
			case RPN_AND: compoundEqOperator = "&="; break;
			case RPN_SHL: compoundEqOperator = "<<="; break;
			case RPN_SHR: compoundEqOperator = ">>="; break;
			default: break;
		}

		warning(
			WARNING_OBSOLETE,
			"`%s %s` is deprecated; use `DEF %s %s`\n",
			$1.c_str(),
			compoundEqOperator,
			$1.c_str(),
			compoundEqOperator
		);
		compoundAssignment($1.c_str(), $2, $3);
	}
;

equs:
	LABEL POP_EQUS string {
		warning(
		    WARNING_OBSOLETE,
		    "`%s EQUS` is deprecated; use `DEF %s EQUS`\n",
		    $1.c_str(),
		    $1.c_str()
		);
		sym_AddString($1.c_str(), $3.string);
	}
;

rb:
	LABEL POP_RB rs_uconst {
		warning(
		    WARNING_OBSOLETE,
		    "`%s RB` is deprecated; use `DEF %s RB`\n",
		    $1.c_str(),
		    $1.c_str()
		);
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + $3);
	}
;

rw:
	LABEL POP_RW rs_uconst {
		warning(
		    WARNING_OBSOLETE,
		    "`%s RW` is deprecated; use `DEF %s RW`\n",
		    $1.c_str(),
		    $1.c_str()
		);
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + 2 * $3);
	}
;

rl:
	LABEL Z80_RL rs_uconst {
		warning(
			WARNING_OBSOLETE,
			"`%s RL` is deprecated; use `DEF %s RL`\n",
			$1.c_str(),
			$1.c_str()
		);
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + 4 * $3);
	}
;

align:
	OP_ALIGN align_spec {
		sect_AlignPC($2.alignment, $2.alignOfs);
	}
;

align_spec:
	uconst {
		if ($1 > 16) {
			::error("Alignment must be between 0 and 16, not %u\n", $1);
			$$.alignment = $$.alignOfs = 0;
		} else {
			$$.alignment = $1;
			$$.alignOfs = 0;
		}
	}
	| uconst COMMA const {
		if ($1 > 16) {
			::error("Alignment must be between 0 and 16, not %u\n", $1);
			$$.alignment = $$.alignOfs = 0;
		} else if ($3 <= -(1 << $1) || $3 >= 1 << $1) {
			::error(
				"The absolute alignment offset (%" PRIu32 ") must be less than alignment size (%d)\n",
				(uint32_t)($3 < 0 ? -$3 : $3),
				1 << $1
			);
			$$.alignment = $$.alignOfs = 0;
		} else {
			$$.alignment = $1;
			$$.alignOfs = $3 < 0 ? (1 << $1) + $3 : $3;
		}
	}
;

opt:
	POP_OPT {
		// Parsing 'opt_list' will restore the lexer's normal mode
		lexer_SetMode(LEXER_RAW);
	} opt_list
;

opt_list:
	  opt_list_entry
	| opt_list opt_list_entry
;

opt_list_entry:
	STRING {
		opt_Parse($1.string);
	}
;

popo:
	POP_POPO {
		opt_Pop();
	}
;

pusho:
	POP_PUSHO {
		opt_Push();
	}
;

pops:
	POP_POPS {
		sect_PopSection();
	}
;

pushs:
	POP_PUSHS {
		sect_PushSection();
	}
;

endsection:
	POP_ENDSECTION {
		sect_EndSection();
	}
;

fail:
	POP_FAIL string {
		fatalerror("%s\n", $2.string);
	}
;

warn:
	POP_WARN string {
		warning(WARNING_USER, "%s\n", $2.string);
	}
;

assert_type:
	%empty {
		$$ = ASSERT_ERROR;
	}
	| POP_WARN COMMA {
		$$ = ASSERT_WARN;
	}
	| POP_FAIL COMMA {
		$$ = ASSERT_ERROR;
	}
	| POP_FATAL COMMA {
		$$ = ASSERT_FATAL;
	}
;

assert:
	POP_ASSERT assert_type relocexpr {
		if (!$3.isKnown) {
			out_CreateAssert($2, $3, "", sect_GetOutputOffset());
		} else if ($3.val == 0) {
			failAssert($2);
		}
		rpn_Free($3);
	}
	| POP_ASSERT assert_type relocexpr COMMA string {
		if (!$3.isKnown) {
			out_CreateAssert($2, $3, $5.string, sect_GetOutputOffset());
		} else if ($3.val == 0) {
			failAssertMsg($2, $5.string);
		}
		rpn_Free($3);
	}
	| POP_STATIC_ASSERT assert_type const {
		if ($3 == 0)
			failAssert($2);
	}
	| POP_STATIC_ASSERT assert_type const COMMA string {
		if ($3 == 0)
			failAssertMsg($2, $5.string);
	}
;

shift:
	POP_SHIFT {
		macro_ShiftCurrentArgs(1);
	}
	| POP_SHIFT const {
		macro_ShiftCurrentArgs($2);
	}
;

load:
	POP_LOAD sectmod string COMMA sectiontype sectorg sectattrs {
		sect_SetLoadSection($3.string, (SectionType)$5, $6, $7, $2);
	}
	| POP_ENDL {
		sect_EndLoadSection();
	}
;

rept:
	POP_REPT uconst NEWLINE capture_rept endofline {
		if ($4)
			fstk_RunRept($2, captureBody.lineNo, captureBody.body, captureBody.size);
	}
;

for:
	POP_FOR {
		lexer_ToggleStringExpansion(false);
	} ID {
		lexer_ToggleStringExpansion(true);
	} COMMA for_args NEWLINE capture_rept endofline {
		if ($8)
			fstk_RunFor(
				$3.c_str(),
				$6.start,
				$6.stop,
				$6.step,
				captureBody.lineNo,
				captureBody.body,
				captureBody.size
			);
	}
;

capture_rept:
	%empty {
		$$ = lexer_CaptureRept(captureBody);
	}
;

for_args:
	const {
		$$.start = 0;
		$$.stop = $1;
		$$.step = 1;
	}
	| const COMMA const {
		$$.start = $1;
		$$.stop = $3;
		$$.step = 1;
	}
	| const COMMA const COMMA const {
		$$.start = $1;
		$$.stop = $3;
		$$.step = $5;
	}
;

break:
	label POP_BREAK endofline {
		if (fstk_Break())
			lexer_SetMode(LEXER_SKIP_TO_ENDR);
	}
;

macrodef:
	POP_MACRO {
		lexer_ToggleStringExpansion(false);
	} ID {
		lexer_ToggleStringExpansion(true);
	} NEWLINE capture_macro endofline {
		if ($6)
			sym_AddMacro($3.c_str(), captureBody.lineNo, captureBody.body, captureBody.size);
	}
;

capture_macro:
	%empty {
		$$ = lexer_CaptureMacroBody(captureBody);
	}
;

rsset:
	POP_RSSET uconst {
		sym_AddVar("_RS", $2);
	}
;

rsreset:
	POP_RSRESET {
		sym_AddVar("_RS", 0);
	}
;

rs_uconst:
	%empty {
		$$ = 1;
	}
	| uconst
;

union:
	POP_UNION {
		sect_StartUnion();
	}
;

nextu:
	POP_NEXTU {
		sect_NextUnionMember();
	}
;

endu:
	POP_ENDU {
		sect_EndUnion();
	}
;

ds:
	POP_DS uconst {
		sect_Skip($2, true);
	}
	| POP_DS uconst COMMA ds_args trailing_comma {
		sect_RelBytes($2, $4);
	}
	| POP_DS OP_ALIGN LBRACK align_spec RBRACK trailing_comma {
		uint32_t n = sect_GetAlignBytes($4.alignment, $4.alignOfs);

		sect_Skip(n, true);
		sect_AlignPC($4.alignment, $4.alignOfs);
	}
	| POP_DS OP_ALIGN LBRACK align_spec RBRACK COMMA ds_args trailing_comma {
		uint32_t n = sect_GetAlignBytes($4.alignment, $4.alignOfs);

		sect_RelBytes(n, $7);
		sect_AlignPC($4.alignment, $4.alignOfs);
	}
;

ds_args:
	reloc_8bit {
		$$.push_back($1);
	}
	| ds_args COMMA reloc_8bit {
		$1.push_back($3);
		$$ = $1;
	}
;

db:
	POP_DB {
		sect_Skip(1, false);
	}
	| POP_DB constlist_8bit trailing_comma
;

dw:
	POP_DW {
		sect_Skip(2, false);
	}
	| POP_DW constlist_16bit trailing_comma
;

dl:
	POP_DL {
		sect_Skip(4, false);
	}
	| POP_DL constlist_32bit trailing_comma
;

def_equ:
	def_id POP_EQU const {
		sym_AddEqu($1.c_str(), $3);
	}
;

redef_equ:
	redef_id POP_EQU const {
		sym_RedefEqu($1.c_str(), $3);
	}
;

def_set:
	def_id POP_EQUAL const {
		sym_AddVar($1.c_str(), $3);
	}
	| redef_id POP_EQUAL const {
		sym_AddVar($1.c_str(), $3);
	}
	| def_id compoundeq const {
		compoundAssignment($1.c_str(), $2, $3);
	}
	| redef_id compoundeq const {
		compoundAssignment($1.c_str(), $2, $3);
	}
;

def_rb:
	def_id POP_RB rs_uconst {
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + $3);
	}
;

def_rw:
	def_id POP_RW rs_uconst {
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + 2 * $3);
	}
;

def_rl:
	def_id Z80_RL rs_uconst {
		sym_AddEqu($1.c_str(), sym_GetConstantValue("_RS"));
		sym_AddVar("_RS", sym_GetConstantValue("_RS") + 4 * $3);
	}
;

def_equs:
	def_id POP_EQUS string {
		sym_AddString($1.c_str(), $3.string);
	}
;

redef_equs:
	redef_id POP_EQUS string {
		sym_RedefString($1.c_str(), $3.string);
	}
;

purge:
	POP_PURGE {
		lexer_ToggleStringExpansion(false);
	} purge_args trailing_comma {
		for (std::string &arg : $3)
			sym_Purge(arg);
		lexer_ToggleStringExpansion(true);
	}
;

purge_args:
	scoped_id {
		$$.push_back($1.c_str());
	}
	| purge_args COMMA scoped_id {
		$1.push_back($3.c_str());
		$$ = $1;
	}
;

export: POP_EXPORT export_list trailing_comma;

export_list:
	  export_list_entry
	| export_list COMMA export_list_entry
;

export_list_entry:
	scoped_id {
		sym_Export($1.c_str());
	}
;

include:
	label POP_INCLUDE string endofline {
		fstk_RunInclude($3.string);
		if (failedOnMissingInclude)
			YYACCEPT;
	}
;

incbin:
	POP_INCBIN string {
		sect_BinaryFile($2.string, 0);
		if (failedOnMissingInclude)
			YYACCEPT;
	}
	| POP_INCBIN string COMMA const {
		sect_BinaryFile($2.string, $4);
		if (failedOnMissingInclude)
			YYACCEPT;
	}
	| POP_INCBIN string COMMA const COMMA const {
		sect_BinaryFileSlice($2.string, $4, $6);
		if (failedOnMissingInclude)
			YYACCEPT;
	}
;

charmap:
	POP_CHARMAP string COMMA const_8bit {
		charmap_Add($2.string, (uint8_t)$4);
	}
;

newcharmap:
	POP_NEWCHARMAP ID {
		charmap_New($2.c_str(), nullptr);
	}
	| POP_NEWCHARMAP ID COMMA ID {
		charmap_New($2.c_str(), $4.c_str());
	}
;

setcharmap:
	POP_SETCHARMAP ID {
		charmap_Set($2.c_str());
	}
;

pushc:
	POP_PUSHC {
		charmap_Push();
	}
;

popc:
	POP_POPC {
		charmap_Pop();
	}
;

print: POP_PRINT print_exprs trailing_comma;

println:
	POP_PRINTLN {
		putchar('\n');
		fflush(stdout);
	}
	| POP_PRINTLN print_exprs trailing_comma {
		putchar('\n');
		fflush(stdout);
	}
;

print_exprs:
	  print_expr
	| print_exprs COMMA print_expr
;

print_expr:
	const_no_str {
		printf("$%" PRIX32, $1);
	}
	| string {
		fputs($1.string, stdout);
	}
;

const_3bit:
	const {
		int32_t value = $1;

		if ((value < 0) || (value > 7)) {
			::error("Immediate value must be 3-bit\n");
			$$ = 0;
		} else {
			$$ = value & 0x7;
		}
	}
;

constlist_8bit:
	  constlist_8bit_entry
	| constlist_8bit COMMA constlist_8bit_entry
;

constlist_8bit_entry:
	reloc_8bit_no_str {
		sect_RelByte($1, 0);
	}
	| string {
		std::vector<uint8_t> output;

		charmap_Convert($1.string, output);
		sect_AbsByteGroup(output.data(), output.size());
	}
;

constlist_16bit:
	  constlist_16bit_entry
	| constlist_16bit COMMA constlist_16bit_entry
;

constlist_16bit_entry:
	reloc_16bit_no_str {
		sect_RelWord($1, 0);
	}
	| string {
		std::vector<uint8_t> output;

		charmap_Convert($1.string, output);
		sect_AbsWordGroup(output.data(), output.size());
	}
;

constlist_32bit:
	  constlist_32bit_entry
	| constlist_32bit COMMA constlist_32bit_entry
;

constlist_32bit_entry:
	relocexpr_no_str {
		sect_RelLong($1, 0);
	}
	| string {
		std::vector<uint8_t> output;

		charmap_Convert($1.string, output);
		sect_AbsLongGroup(output.data(), output.size());
	}
;

reloc_8bit:
	relocexpr {
		rpn_CheckNBit($1, 8);
		$$ = $1;
	}
;

reloc_8bit_no_str:
	relocexpr_no_str {
		rpn_CheckNBit($1, 8);
		$$ = $1;
	}
;

reloc_8bit_offset:
	OP_ADD relocexpr {
		rpn_CheckNBit($2, 8);
		$$ = $2;
	}
	| OP_SUB relocexpr {
		rpn_NEG($$, $2);
		rpn_CheckNBit($$, 8);
	}
;

reloc_16bit:
	relocexpr {
		rpn_CheckNBit($1, 16);
		$$ = $1;
	}
;

reloc_16bit_no_str:
	relocexpr_no_str {
		rpn_CheckNBit($1, 16);
		$$ = $1;
	}
;

relocexpr:
	  relocexpr_no_str
	| string {
		std::vector<uint8_t> output;

		charmap_Convert($1.string, output);
		rpn_Number($$, str2int2(output));
	}
;

relocexpr_no_str:
	scoped_anon_id {
		rpn_Symbol($$, $1.c_str());
	}
	| NUMBER {
		rpn_Number($$, $1);
	}
	| OP_LOGICNOT relocexpr %prec NEG {
		rpn_LOGNOT($$, $2);
	}
	| relocexpr OP_LOGICOR relocexpr {
		rpn_BinaryOp(RPN_LOGOR, $$, $1, $3);
	}
	| relocexpr OP_LOGICAND relocexpr {
		rpn_BinaryOp(RPN_LOGAND, $$, $1, $3);
	}
	| relocexpr OP_LOGICEQU relocexpr {
		rpn_BinaryOp(RPN_LOGEQ, $$, $1, $3);
	}
	| relocexpr OP_LOGICGT relocexpr {
		rpn_BinaryOp(RPN_LOGGT, $$, $1, $3);
	}
	| relocexpr OP_LOGICLT relocexpr {
		rpn_BinaryOp(RPN_LOGLT, $$, $1, $3);
	}
	| relocexpr OP_LOGICGE relocexpr {
		rpn_BinaryOp(RPN_LOGGE, $$, $1, $3);
	}
	| relocexpr OP_LOGICLE relocexpr {
		rpn_BinaryOp(RPN_LOGLE, $$, $1, $3);
	}
	| relocexpr OP_LOGICNE relocexpr {
		rpn_BinaryOp(RPN_LOGNE, $$, $1, $3);
	}
	| relocexpr OP_ADD relocexpr {
		rpn_BinaryOp(RPN_ADD, $$, $1, $3);
	}
	| relocexpr OP_SUB relocexpr {
		rpn_BinaryOp(RPN_SUB, $$, $1, $3);
	}
	| relocexpr OP_XOR relocexpr {
		rpn_BinaryOp(RPN_XOR, $$, $1, $3);
	}
	| relocexpr OP_OR relocexpr {
		rpn_BinaryOp(RPN_OR, $$, $1, $3);
	}
	| relocexpr OP_AND relocexpr {
		rpn_BinaryOp(RPN_AND, $$, $1, $3);
	}
	| relocexpr OP_SHL relocexpr {
		rpn_BinaryOp(RPN_SHL, $$, $1, $3);
	}
	| relocexpr OP_SHR relocexpr {
		rpn_BinaryOp(RPN_SHR, $$, $1, $3);
	}
	| relocexpr OP_USHR relocexpr {
		rpn_BinaryOp(RPN_USHR, $$, $1, $3);
	}
	| relocexpr OP_MUL relocexpr {
		rpn_BinaryOp(RPN_MUL, $$, $1, $3);
	}
	| relocexpr OP_DIV relocexpr {
		rpn_BinaryOp(RPN_DIV, $$, $1, $3);
	}
	| relocexpr OP_MOD relocexpr {
		rpn_BinaryOp(RPN_MOD, $$, $1, $3);
	}
	| relocexpr OP_EXP relocexpr {
		rpn_BinaryOp(RPN_EXP, $$, $1, $3);
	}
	| OP_ADD relocexpr %prec NEG {
		$$ = $2;
	}
	| OP_SUB relocexpr %prec NEG {
		rpn_NEG($$, $2);
	}
	| OP_NOT relocexpr %prec NEG {
		rpn_NOT($$, $2);
	}
	| OP_HIGH LPAREN relocexpr RPAREN {
		rpn_HIGH($$, $3);
	}
	| OP_LOW LPAREN relocexpr RPAREN {
		rpn_LOW($$, $3);
	}
	| OP_ISCONST LPAREN relocexpr RPAREN {
		rpn_ISCONST($$, $3);
	}
	| OP_BANK LPAREN scoped_anon_id RPAREN {
		// '@' is also an ID; it is handled here
		rpn_BankSymbol($$, $3.c_str());
	}
	| OP_BANK LPAREN string RPAREN {
		rpn_BankSection($$, $3.string);
	}
	| OP_SIZEOF LPAREN string RPAREN {
		rpn_SizeOfSection($$, $3.string);
	}
	| OP_STARTOF LPAREN string RPAREN {
		rpn_StartOfSection($$, $3.string);
	}
	| OP_SIZEOF LPAREN sectiontype RPAREN {
		rpn_SizeOfSectionType($$, (SectionType)$3);
	}
	| OP_STARTOF LPAREN sectiontype RPAREN {
		rpn_StartOfSectionType($$, (SectionType)$3);
	}
	| OP_DEF {
		lexer_ToggleStringExpansion(false);
	} LPAREN scoped_anon_id RPAREN {
		rpn_Number($$, sym_FindScopedValidSymbol($4.c_str()) != nullptr);
		lexer_ToggleStringExpansion(true);
	}
	| OP_ROUND LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Round($3, $4));
	}
	| OP_CEIL LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Ceil($3, $4));
	}
	| OP_FLOOR LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Floor($3, $4));
	}
	| OP_FDIV LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_Div($3, $5, $6));
	}
	| OP_FMUL LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_Mul($3, $5, $6));
	}
	| OP_FMOD LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_Mod($3, $5, $6));
	}
	| OP_POW LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_Pow($3, $5, $6));
	}
	| OP_LOG LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_Log($3, $5, $6));
	}
	| OP_SIN LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Sin($3, $4));
	}
	| OP_COS LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Cos($3, $4));
	}
	| OP_TAN LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_Tan($3, $4));
	}
	| OP_ASIN LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_ASin($3, $4));
	}
	| OP_ACOS LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_ACos($3, $4));
	}
	| OP_ATAN LPAREN const opt_q_arg RPAREN {
		rpn_Number($$, fix_ATan($3, $4));
	}
	| OP_ATAN2 LPAREN const COMMA const opt_q_arg RPAREN {
		rpn_Number($$, fix_ATan2($3, $5, $6));
	}
	| OP_STRCMP LPAREN string COMMA string RPAREN {
		rpn_Number($$, strcmp($3.string, $5.string));
	}
	| OP_STRIN LPAREN string COMMA string RPAREN {
		char const *p = strstr($3.string, $5.string);

		rpn_Number($$, p ? p - $3.string + 1 : 0);
	}
	| OP_STRRIN LPAREN string COMMA string RPAREN {
		char const *p = strrstr($3.string, $5.string);

		rpn_Number($$, p ? p - $3.string + 1 : 0);
	}
	| OP_STRLEN LPAREN string RPAREN {
		rpn_Number($$, strlenUTF8($3.string));
	}
	| OP_CHARLEN LPAREN string RPAREN {
		rpn_Number($$, charlenUTF8($3.string));
	}
	| OP_INCHARMAP LPAREN string RPAREN {
		rpn_Number($$, charmap_HasChar($3.string));
	}
	| LPAREN relocexpr RPAREN {
		$$ = $2;
	}
;

uconst:
	const {
		if ($1 < 0)
			fatalerror("Constant must not be negative: %d\n", $1);
		$$ = $1;
	}
;

const:
	relocexpr {
		$$ = $1.getConstVal();
	}
;

const_no_str:
	relocexpr_no_str {
		$$ = $1.getConstVal();
	}
;

const_8bit:
	reloc_8bit {
		$$ = $1.getConstVal();
	}
;

opt_q_arg:
	%empty {
		$$ = fix_Precision();
	}
	| COMMA const {
		if ($2 >= 1 && $2 <= 31) {
			$$ = $2;
		} else {
			::error("Fixed-point precision must be between 1 and 31\n");
			$$ = fix_Precision();
		}
	}
;

string:
	  STRING
	| OP_STRSUB LPAREN string COMMA const COMMA uconst RPAREN {
		size_t len = strlenUTF8($3.string);
		uint32_t pos = adjustNegativePos($5, len, "STRSUB");

		strsubUTF8($$.string, sizeof($$.string), $3.string, pos, $7);
	}
	| OP_STRSUB LPAREN string COMMA const RPAREN {
		size_t len = strlenUTF8($3.string);
		uint32_t pos = adjustNegativePos($5, len, "STRSUB");

		strsubUTF8($$.string, sizeof($$.string), $3.string, pos, pos > len ? 0 : len + 1 - pos);
	}
	| OP_CHARSUB LPAREN string COMMA const RPAREN {
		size_t len = charlenUTF8($3.string);
		uint32_t pos = adjustNegativePos($5, len, "CHARSUB");

		charsubUTF8($$.string, $3.string, pos);
	}
	| OP_STRCAT LPAREN RPAREN {
		$$.string[0] = '\0';
	}
	| OP_STRCAT LPAREN strcat_args RPAREN {
		$$ = $3;
	}
	| OP_STRUPR LPAREN string RPAREN {
		upperstring($$.string, $3.string);
	}
	| OP_STRLWR LPAREN string RPAREN {
		lowerstring($$.string, $3.string);
	}
	| OP_STRRPL LPAREN string COMMA string COMMA string RPAREN {
		strrpl($$.string, sizeof($$.string), $3.string, $5.string, $7.string);
	}
	| OP_STRFMT LPAREN strfmt_args RPAREN {
		strfmt($$.string, sizeof($$.string), $3.format.c_str(), $3.args);
	}
	| POP_SECTION LPAREN scoped_anon_id RPAREN {
		Symbol *sym = sym_FindScopedValidSymbol($3.c_str());

		if (!sym)
			fatalerror("Unknown symbol \"%s\"\n", $3.c_str());
		Section const *section = sym->getSection();

		if (!section)
			fatalerror("\"%s\" does not belong to any section\n", sym->name.c_str());
		// Section names are capped by rgbasm's maximum string length,
		// so this currently can't overflow.
		strcpy($$.string, section->name.c_str());
	}
;

strcat_args:
	  string
	| strcat_args COMMA string {
		int ret = snprintf($$.string, sizeof($$.string), "%s%s", $1.string, $3.string);

		if (ret == -1)
			fatalerror("snprintf error in STRCAT: %s\n", strerror(errno));
		else if ((unsigned int)ret >= sizeof($$.string))
			warning(WARNING_LONG_STR, "STRCAT: String too long '%s%s'\n", $1.string, $3.string);
	}
;

strfmt_args:
	string strfmt_va_args {
		$$ = $2;
		$$.format = $1.string;
	}
;

strfmt_va_args:
	  %empty {}
	| strfmt_va_args COMMA const_no_str {
		$1.args.push_back((uint32_t)$3);
		$$ = $1;
	}
	| strfmt_va_args COMMA string {
		$1.args.push_back($3.string);
		$$ = $1;
	}
;

section:
	POP_SECTION sectmod string COMMA sectiontype sectorg sectattrs {
		sect_NewSection($3.string, (SectionType)$5, $6, $7, $2);
	}
;

sectmod:
	%empty {
		$$ = SECTION_NORMAL;
	}
	| POP_UNION {
		$$ = SECTION_UNION;
	}
	| POP_FRAGMENT {
		$$ = SECTION_FRAGMENT;
	}
;

sectiontype:
	SECT_WRAM0 {
		$$ = SECTTYPE_WRAM0;
	}
	| SECT_VRAM {
		$$ = SECTTYPE_VRAM;
	}
	| SECT_ROMX {
		$$ = SECTTYPE_ROMX;
	}
	| SECT_ROM0 {
		$$ = SECTTYPE_ROM0;
	}
	| SECT_HRAM {
		$$ = SECTTYPE_HRAM;
	}
	| SECT_WRAMX {
		$$ = SECTTYPE_WRAMX;
	}
	| SECT_SRAM {
		$$ = SECTTYPE_SRAM;
	}
	| SECT_OAM {
		$$ = SECTTYPE_OAM;
	}
;

sectorg:
	%empty {
		$$ = -1;
	}
	| LBRACK uconst RBRACK {
		if ($2 < 0 || $2 >= 0x10000) {
			::error("Address $%x is not 16-bit\n", $2);
			$$ = -1;
		} else {
			$$ = $2;
		}
	}
;

sectattrs:
	%empty {
		$$.alignment = 0;
		$$.alignOfs = 0;
		$$.bank = -1;
	}
	| sectattrs COMMA OP_ALIGN LBRACK align_spec RBRACK {
		$$ = $1;
		$$.alignment = $5.alignment;
		$$.alignOfs = $5.alignOfs;
	}
	| sectattrs COMMA OP_BANK LBRACK uconst RBRACK {
		$$ = $1;
		$$.bank = $5; // We cannot check the validity of this yet
	}
;

// CPU commands.

cpu_commands:
	  cpu_command
	| cpu_command DOUBLE_COLON cpu_commands
;

cpu_command:
	  z80_adc
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

z80_adc:
	Z80_ADC op_a_n {
		sect_AbsByte(0xCE);
		sect_RelByte($2, 1);
	}
	| Z80_ADC op_a_r {
		sect_AbsByte(0x88 | $2);
	}
;

z80_add:
	Z80_ADD op_a_n {
		sect_AbsByte(0xC6);
		sect_RelByte($2, 1);
	}
	| Z80_ADD op_a_r {
		sect_AbsByte(0x80 | $2);
	}
	| Z80_ADD MODE_HL COMMA reg_ss {
		sect_AbsByte(0x09 | ($4 << 4));
	}
	| Z80_ADD MODE_SP COMMA reloc_8bit {
		sect_AbsByte(0xE8);
		sect_RelByte($4, 1);
	}
;

z80_and:
	Z80_AND op_a_n {
		sect_AbsByte(0xE6);
		sect_RelByte($2, 1);
	}
	| Z80_AND op_a_r {
		sect_AbsByte(0xA0 | $2);
	}
;

z80_bit:
	Z80_BIT const_3bit COMMA reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x40 | ($2 << 3) | $4);
	}
;

z80_call:
	Z80_CALL reloc_16bit {
		sect_AbsByte(0xCD);
		sect_RelWord($2, 1);
	}
	| Z80_CALL ccode_expr COMMA reloc_16bit {
		sect_AbsByte(0xC4 | ($2 << 3));
		sect_RelWord($4, 1);
	}
;

z80_ccf:
	Z80_CCF {
		sect_AbsByte(0x3F);
	}
;

z80_cp:
	Z80_CP op_a_n {
		sect_AbsByte(0xFE);
		sect_RelByte($2, 1);
	}
	| Z80_CP op_a_r {
		sect_AbsByte(0xB8 | $2);
	}
;

z80_cpl:
	Z80_CPL {
		sect_AbsByte(0x2F);
	}
;

z80_daa:
	Z80_DAA {
		sect_AbsByte(0x27);
	}
;

z80_dec:
	Z80_DEC reg_r {
		sect_AbsByte(0x05 | ($2 << 3));
	}
	| Z80_DEC reg_ss {
		sect_AbsByte(0x0B | ($2 << 4));
	}
;

z80_di:
	Z80_DI {
		sect_AbsByte(0xF3);
	}
;

z80_ei:
	Z80_EI {
		sect_AbsByte(0xFB);
	}
;

z80_halt:
	Z80_HALT {
		sect_AbsByte(0x76);
		if (haltNop) {
			if (warnOnHaltNop) {
				warnOnHaltNop = false;
				warning(
					WARNING_OBSOLETE,
					"Automatic `nop` after `halt` (option 'H') is deprecated\n"
				);
			}
			sect_AbsByte(0x00);
		}
	}
;

z80_inc:
	Z80_INC reg_r {
		sect_AbsByte(0x04 | ($2 << 3));
	}
	| Z80_INC reg_ss {
		sect_AbsByte(0x03 | ($2 << 4));
	}
;

z80_jp:
	Z80_JP reloc_16bit {
		sect_AbsByte(0xC3);
		sect_RelWord($2, 1);
	}
	| Z80_JP ccode_expr COMMA reloc_16bit {
		sect_AbsByte(0xC2 | ($2 << 3));
		sect_RelWord($4, 1);
	}
	| Z80_JP MODE_HL {
		sect_AbsByte(0xE9);
	}
;

z80_jr:
	Z80_JR reloc_16bit {
		sect_AbsByte(0x18);
		sect_PCRelByte($2, 1);
	}
	| Z80_JR ccode_expr COMMA reloc_16bit {
		sect_AbsByte(0x20 | ($2 << 3));
		sect_PCRelByte($4, 1);
	}
;

z80_ldi:
	Z80_LDI LBRACK MODE_HL RBRACK COMMA MODE_A {
		sect_AbsByte(0x02 | (2 << 4));
	}
	| Z80_LDI MODE_A COMMA LBRACK MODE_HL RBRACK {
		sect_AbsByte(0x0A | (2 << 4));
	}
;

z80_ldd:
	Z80_LDD LBRACK MODE_HL RBRACK COMMA MODE_A {
		sect_AbsByte(0x02 | (3 << 4));
	}
	| Z80_LDD MODE_A COMMA LBRACK MODE_HL RBRACK {
		sect_AbsByte(0x0A | (3 << 4));
	}
;

z80_ldio:
	Z80_LDH MODE_A COMMA op_mem_ind {
		rpn_CheckHRAM($4, $4);

		sect_AbsByte(0xF0);
		sect_RelByte($4, 1);
	}
	| Z80_LDH op_mem_ind COMMA MODE_A {
		rpn_CheckHRAM($2, $2);

		sect_AbsByte(0xE0);
		sect_RelByte($2, 1);
	}
	| Z80_LDH MODE_A COMMA c_ind {
		sect_AbsByte(0xF2);
	}
	| Z80_LDH c_ind COMMA MODE_A {
		sect_AbsByte(0xE2);
	}
;

c_ind:
	  LBRACK MODE_C RBRACK
	| LBRACK relocexpr OP_ADD MODE_C RBRACK {
		if (!$2.isKnown || $2.val != 0xFF00)
			::error("Expected constant expression equal to $FF00 for \"$ff00+c\"\n");
	}
;

z80_ld:
	  z80_ld_mem
	| z80_ld_cind
	| z80_ld_rr
	| z80_ld_ss
	| z80_ld_hl
	| z80_ld_sp
	| z80_ld_r
	| z80_ld_a
;

z80_ld_hl:
	Z80_LD MODE_HL COMMA MODE_SP reloc_8bit_offset {
		sect_AbsByte(0xF8);
		sect_RelByte($5, 1);
	}
	| Z80_LD MODE_HL COMMA reloc_16bit {
		sect_AbsByte(0x01 | (REG_HL << 4));
		sect_RelWord($4, 1);
	}
;

z80_ld_sp:
	Z80_LD MODE_SP COMMA MODE_HL {
		sect_AbsByte(0xF9);
	}
	| Z80_LD MODE_SP COMMA reloc_16bit {
		sect_AbsByte(0x01 | (REG_SP << 4));
		sect_RelWord($4, 1);
	}
;

z80_ld_mem:
	Z80_LD op_mem_ind COMMA MODE_SP {
		sect_AbsByte(0x08);
		sect_RelWord($2, 1);
	}
	| Z80_LD op_mem_ind COMMA MODE_A {
		if (optimizeLoads && $2.isKnown && $2.val >= 0xFF00) {
			if (warnOnLdOpt) {
				warnOnLdOpt = false;
				warning(
					WARNING_OBSOLETE,
					"Automatic `ld` to `ldh` optimization (option 'l') is deprecated\n"
				);
			}
			sect_AbsByte(0xE0);
			sect_AbsByte($2.val & 0xFF);
			rpn_Free($2);
		} else {
			sect_AbsByte(0xEA);
			sect_RelWord($2, 1);
		}
	}
;

z80_ld_cind:
	Z80_LD c_ind COMMA MODE_A {
		sect_AbsByte(0xE2);
	}
;

z80_ld_rr:
	Z80_LD reg_rr COMMA MODE_A {
		sect_AbsByte(0x02 | ($2 << 4));
	}
;

z80_ld_r:
	Z80_LD reg_r COMMA reloc_8bit {
		sect_AbsByte(0x06 | ($2 << 3));
		sect_RelByte($4, 1);
	}
	| Z80_LD reg_r COMMA reg_r {
		if (($2 == REG_HL_IND) && ($4 == REG_HL_IND))
			::error("LD [HL],[HL] not a valid instruction\n");
		else
			sect_AbsByte(0x40 | ($2 << 3) | $4);
	}
;

z80_ld_a:
	Z80_LD reg_r COMMA c_ind {
		if ($2 == REG_A)
			sect_AbsByte(0xF2);
		else
			::error("Destination operand must be A\n");
	}
	| Z80_LD reg_r COMMA reg_rr {
		if ($2 == REG_A)
			sect_AbsByte(0x0A | ($4 << 4));
		else
			::error("Destination operand must be A\n");
	}
	| Z80_LD reg_r COMMA op_mem_ind {
		if ($2 == REG_A) {
			if (optimizeLoads && $4.isKnown && $4.val >= 0xFF00) {
				if (warnOnLdOpt) {
					warnOnLdOpt = false;
					warning(
						WARNING_OBSOLETE,
						"Automatic `ld` to `ldh` optimization (option 'l') is deprecated\n"
					);
				}
				sect_AbsByte(0xF0);
				sect_AbsByte($4.val & 0xFF);
				rpn_Free($4);
			} else {
				sect_AbsByte(0xFA);
				sect_RelWord($4, 1);
			}
		} else {
			::error("Destination operand must be A\n");
			rpn_Free($4);
		}
	}
;

z80_ld_ss:
	Z80_LD MODE_BC COMMA reloc_16bit {
		sect_AbsByte(0x01 | (REG_BC << 4));
		sect_RelWord($4, 1);
	}
	| Z80_LD MODE_DE COMMA reloc_16bit {
		sect_AbsByte(0x01 | (REG_DE << 4));
		sect_RelWord($4, 1);
	}
	// HL is taken care of in z80_ld_hl
	// SP is taken care of in z80_ld_sp
;

z80_nop:
	Z80_NOP {
		sect_AbsByte(0x00);
	}
;

z80_or:
	Z80_OR op_a_n {
		sect_AbsByte(0xF6);
		sect_RelByte($2, 1);
	}
	| Z80_OR op_a_r {
		sect_AbsByte(0xB0 | $2);
	}
;

z80_pop:
	Z80_POP reg_tt {
		sect_AbsByte(0xC1 | ($2 << 4));
	}
;

z80_push:
	Z80_PUSH reg_tt {
		sect_AbsByte(0xC5 | ($2 << 4));
	}
;

z80_res:
	Z80_RES const_3bit COMMA reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x80 | ($2 << 3) | $4);
	}
;

z80_ret:
	Z80_RET {
		sect_AbsByte(0xC9);
	}
	| Z80_RET ccode_expr {
		sect_AbsByte(0xC0 | ($2 << 3));
	}
;

z80_reti:
	Z80_RETI {
		sect_AbsByte(0xD9);
	}
;

z80_rl:
	Z80_RL reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x10 | $2);
	}
;

z80_rla:
	Z80_RLA {
		sect_AbsByte(0x17);
	}
;

z80_rlc:
	Z80_RLC reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x00 | $2);
	}
;

z80_rlca:
	Z80_RLCA {
		sect_AbsByte(0x07);
	}
;

z80_rr:
	Z80_RR reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x18 | $2);
	}
;

z80_rra:
	Z80_RRA {
		sect_AbsByte(0x1F);
	}
;

z80_rrc:
	Z80_RRC reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x08 | $2);
	}
;

z80_rrca:
	Z80_RRCA {
		sect_AbsByte(0x0F);
	}
;

z80_rst:
	Z80_RST reloc_8bit {
		rpn_CheckRST($2, $2);
		if (!$2.isKnown)
			sect_RelByte($2, 0);
		else
			sect_AbsByte(0xC7 | $2.val);
		rpn_Free($2);
	}
;

z80_sbc:
	Z80_SBC op_a_n {
		sect_AbsByte(0xDE);
		sect_RelByte($2, 1);
	}
	| Z80_SBC op_a_r {
		sect_AbsByte(0x98 | $2);
	}
;

z80_scf:
	Z80_SCF {
		sect_AbsByte(0x37);
	}
;

z80_set:
	Z80_SET const_3bit COMMA reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0xC0 | ($2 << 3) | $4);
	}
;

z80_sla:
	Z80_SLA reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x20 | $2);
	}
;

z80_sra:
	Z80_SRA reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x28 | $2);
	}
;

z80_srl:
	Z80_SRL reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x38 | $2);
	}
;

z80_stop:
	Z80_STOP {
		sect_AbsByte(0x10);
		sect_AbsByte(0x00);
	}
	| Z80_STOP reloc_8bit {
		sect_AbsByte(0x10);
		sect_RelByte($2, 1);
	}
;

z80_sub:
	Z80_SUB op_a_n {
		sect_AbsByte(0xD6);
		sect_RelByte($2, 1);
	}
	| Z80_SUB op_a_r {
		sect_AbsByte(0x90 | $2);
	}
;

z80_swap:
	Z80_SWAP reg_r {
		sect_AbsByte(0xCB);
		sect_AbsByte(0x30 | $2);
	}
;

z80_xor:
	Z80_XOR op_a_n {
		sect_AbsByte(0xEE);
		sect_RelByte($2, 1);
	}
	| Z80_XOR op_a_r {
		sect_AbsByte(0xA8 | $2);
	}
;

// Registers or values.

op_mem_ind:
	LBRACK reloc_16bit RBRACK {
		$$ = $2;
	}
;

op_a_r:
	  reg_r
	| MODE_A COMMA reg_r {
		$$ = $3;
	}
;

op_a_n:
	  reloc_8bit
	| MODE_A COMMA reloc_8bit {
		$$ = $3;
	}
;

// Registers and condition codes.

MODE_A:
	  TOKEN_A
	| OP_HIGH LPAREN MODE_AF RPAREN
;

MODE_B:
	  TOKEN_B
	| OP_HIGH LPAREN MODE_BC RPAREN
;

MODE_C:
	  TOKEN_C
	| OP_LOW LPAREN MODE_BC RPAREN
;

MODE_D:
	  TOKEN_D
	| OP_HIGH LPAREN MODE_DE RPAREN
;

MODE_E:
	  TOKEN_E
	| OP_LOW LPAREN MODE_DE RPAREN
;

MODE_H:
	  TOKEN_H
	| OP_HIGH LPAREN MODE_HL RPAREN
;

MODE_L:
	  TOKEN_L
	| OP_LOW LPAREN MODE_HL RPAREN
;

ccode_expr:
	  ccode
	| OP_LOGICNOT ccode_expr {
		$$ = $2 ^ 1;
	}
;

ccode:
	CC_NZ {
		$$ = CC_NZ;
	}
	| CC_Z {
		$$ = CC_Z;
	}
	| CC_NC {
		$$ = CC_NC;
	}
	| TOKEN_C {
		$$ = CC_C;
	}
;

reg_r:
	MODE_B {
		$$ = REG_B;
	}
	| MODE_C {
		$$ = REG_C;
	}
	| MODE_D {
		$$ = REG_D;
	}
	| MODE_E {
		$$ = REG_E;
	}
	| MODE_H {
		$$ = REG_H;
	}
	| MODE_L {
		$$ = REG_L;
	}
	| LBRACK MODE_HL RBRACK {
		$$ = REG_HL_IND;
	}
	| MODE_A {
		$$ = REG_A;
	}
;

reg_tt:
	MODE_BC {
		$$ = REG_BC;
	}
	| MODE_DE {
		$$ = REG_DE;
	}
	| MODE_HL {
		$$ = REG_HL;
	}
	| MODE_AF {
		$$ = REG_AF;
	}
;

reg_ss:
	MODE_BC {
		$$ = REG_BC;
	}
	| MODE_DE {
		$$ = REG_DE;
	}
	| MODE_HL {
		$$ = REG_HL;
	}
	| MODE_SP {
		$$ = REG_SP;
	}
;

reg_rr:
	LBRACK MODE_BC RBRACK {
		$$ = REG_BC_IND;
	}
	| LBRACK MODE_DE RBRACK {
		$$ = REG_DE_IND;
	}
	| hl_ind_inc {
		$$ = REG_HL_INDINC;
	}
	| hl_ind_dec {
		$$ = REG_HL_INDDEC;
	}
;

hl_ind_inc:
	  LBRACK MODE_HL_INC RBRACK
	| LBRACK MODE_HL OP_ADD RBRACK
;

hl_ind_dec:
	  LBRACK MODE_HL_DEC RBRACK
	| LBRACK MODE_HL OP_SUB RBRACK
;

%%

// Semantic actions.

static void upperstring(char *dest, char const *src) {
	while (*src)
		*dest++ = toupper(*src++);
	*dest = '\0';
}

static void lowerstring(char *dest, char const *src) {
	while (*src)
		*dest++ = tolower(*src++);
	*dest = '\0';
}

static uint32_t str2int2(std::vector<uint8_t> const &s) {
	uint32_t length = s.size();

	if (length > 4)
		warning(
		    WARNING_NUMERIC_STRING_1,
		    "Treating string as a number ignores first %" PRIu32 " character%s\n", length - 4,
		    length == 5 ? "" : "s"
		);
	else if (length > 1)
		warning(
		    WARNING_NUMERIC_STRING_2, "Treating %" PRIu32 "-character string as a number\n", length
		);

	uint32_t r = 0;

	for (uint32_t i = length < 4 ? 0 : length - 4; i < length; i++) {
		r <<= 8;
		r |= s[i];
	}

	return r;
}

static const char *strrstr(char const *s1, char const *s2) {
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);

	if (len2 > len1)
		return nullptr;

	for (char const *p = s1 + len1 - len2; p >= s1; p--)
		if (!strncmp(p, s2, len2))
			return p;

	return nullptr;
}

static void errorInvalidUTF8Byte(uint8_t byte, char const *functionName) {
	error("%s: Invalid UTF-8 byte 0x%02hhX\n", functionName, byte);
}

static size_t strlenUTF8(char const *s) {
	size_t len = 0;
	uint32_t state = 0;

	for (uint32_t codep = 0; *s; s++) {
		uint8_t byte = *s;

		switch (decode(&state, &codep, byte)) {
		case 1:
			errorInvalidUTF8Byte(byte, "STRLEN");
			state = 0;
			// fallthrough
		case 0:
			len++;
			break;
		}
	}

	// Check for partial code point.
	if (state != 0)
		error("STRLEN: Incomplete UTF-8 character\n");

	return len;
}

static void strsubUTF8(char *dest, size_t destLen, char const *src, uint32_t pos, uint32_t len) {
	size_t srcIndex = 0;
	size_t destIndex = 0;
	uint32_t state = 0;
	uint32_t codep = 0;
	uint32_t curLen = 0;
	uint32_t curPos = 1;

	// Advance to starting position in source string.
	while (src[srcIndex] && curPos < pos) {
		switch (decode(&state, &codep, src[srcIndex])) {
		case 1:
			errorInvalidUTF8Byte(src[srcIndex], "STRSUB");
			state = 0;
			// fallthrough
		case 0:
			curPos++;
			break;
		}
		srcIndex++;
	}

	// A position 1 past the end of the string is allowed, but will trigger the
	// "Length too big" warning below if the length is nonzero.
	if (!src[srcIndex] && pos > curPos)
		warning(
		    WARNING_BUILTIN_ARG, "STRSUB: Position %" PRIu32 " is past the end of the string\n", pos
		);

	// Copy from source to destination.
	while (src[srcIndex] && destIndex < destLen - 1 && curLen < len) {
		switch (decode(&state, &codep, src[srcIndex])) {
		case 1:
			errorInvalidUTF8Byte(src[srcIndex], "STRSUB");
			state = 0;
			// fallthrough
		case 0:
			curLen++;
			break;
		}
		dest[destIndex++] = src[srcIndex++];
	}

	if (curLen < len)
		warning(WARNING_BUILTIN_ARG, "STRSUB: Length too big: %" PRIu32 "\n", len);

	// Check for partial code point.
	if (state != 0)
		error("STRSUB: Incomplete UTF-8 character\n");

	dest[destIndex] = '\0';
}

static size_t charlenUTF8(char const *str) {
	size_t len;

	for (len = 0; charmap_ConvertNext(str, nullptr); len++)
		;

	return len;
}

static void charsubUTF8(char *dest, char const *src, uint32_t pos) {
	size_t charLen = 1;

	// Advance to starting position in source string.
	for (uint32_t curPos = 1; charLen && curPos < pos; curPos++)
		charLen = charmap_ConvertNext(src, nullptr);

	char const *start = src;

	if (!charmap_ConvertNext(src, nullptr))
		warning(
		    WARNING_BUILTIN_ARG, "CHARSUB: Position %" PRIu32 " is past the end of the string\n",
		    pos
		);

	// Copy from source to destination.
	memcpy(dest, start, src - start);

	dest[src - start] = '\0';
}

static uint32_t adjustNegativePos(int32_t pos, size_t len, char const *functionName) {
	// STRSUB and CHARSUB adjust negative `pos` arguments the same way,
	// such that position -1 is the last character of a string.
	if (pos < 0)
		pos += len + 1;
	if (pos < 1) {
		warning(WARNING_BUILTIN_ARG, "%s: Position starts at 1\n", functionName);
		pos = 1;
	}
	return (uint32_t)pos;
}

static void strrpl(char *dest, size_t destLen, char const *src, char const *old, char const *rep) {
	size_t oldLen = strlen(old);
	size_t repLen = strlen(rep);
	size_t i = 0;

	if (!oldLen) {
		warning(WARNING_EMPTY_STRRPL, "STRRPL: Cannot replace an empty string\n");
		strcpy(dest, src);
		return;
	}

	for (char const *next = strstr(src, old); next && *next; next = strstr(src, old)) {
		// Copy anything before the substring to replace
		unsigned int lenBefore = next - src;

		memcpy(dest + i, src, lenBefore < destLen - i ? lenBefore : destLen - i);
		i += next - src;
		if (i >= destLen)
			break;

		// Copy the replacement substring
		memcpy(dest + i, rep, repLen < destLen - i ? repLen : destLen - i);
		i += repLen;
		if (i >= destLen)
			break;

		src = next + oldLen;
	}

	if (i < destLen) {
		size_t srcLen = strlen(src);

		// Copy anything after the last replaced substring
		memcpy(dest + i, src, srcLen < destLen - i ? srcLen : destLen - i);
		i += srcLen;
	}

	if (i >= destLen) {
		warning(WARNING_LONG_STR, "STRRPL: String too long, got truncated\n");
		i = destLen - 1;
	}
	dest[i] = '\0';
}

static void strfmt(
    char *dest, size_t destLen, char const *spec,
    std::vector<std::variant<uint32_t, std::string>> &args
) {
	size_t a = 0;
	size_t i = 0;

	while (i < destLen) {
		int c = *spec++;

		if (c == '\0') {
			break;
		} else if (c != '%') {
			dest[i++] = c;
			continue;
		}

		c = *spec++;

		if (c == '%') {
			dest[i++] = c;
			continue;
		}

		FormatSpec fmt{};

		while (c != '\0') {
			fmt.useCharacter(c);
			if (fmt.isFinished())
				break;
			c = *spec++;
		}

		if (fmt.isEmpty()) {
			error("STRFMT: Illegal '%%' at end of format string\n");
			dest[i++] = '%';
			break;
		} else if (!fmt.isValid()) {
			error("STRFMT: Invalid format spec for argument %zu\n", a + 1);
			dest[i++] = '%';
			a++;
			continue;
		} else if (a >= args.size()) {
			// Will warn after formatting is done.
			dest[i++] = '%';
			a++;
			continue;
		}

		std::variant<uint32_t, std::string> &arg = args[a++];
		static char buf[MAXSTRLEN + 1];

		std::visit(
		    Visitor{
		        [&](uint32_t num) { fmt.printNumber(buf, sizeof(buf), num); },
		        [&](std::string &str) { fmt.printString(buf, sizeof(buf), str.c_str()); },
		    },
		    arg
		);

		i += snprintf(&dest[i], destLen - i, "%s", buf);
	}

	if (a < args.size())
		error("STRFMT: %zu unformatted argument(s)\n", args.size() - a);
	else if (a > args.size())
		error(
		    "STRFMT: Not enough arguments for format spec, got: %zu, need: %zu\n", args.size(), a
		);

	if (i > destLen - 1) {
		warning(WARNING_LONG_STR, "STRFMT: String too long, got truncated\n");
		i = destLen - 1;
	}
	dest[i] = '\0';
}

static void compoundAssignment(const char *symName, RPNCommand op, int32_t constValue) {
	Expression oldExpr, constExpr, newExpr;
	int32_t newValue;

	rpn_Symbol(oldExpr, symName);
	rpn_Number(constExpr, constValue);
	rpn_BinaryOp(op, newExpr, oldExpr, constExpr);
	newValue = newExpr.getConstVal();
	sym_AddVar(symName, newValue);
}

static void failAssert(AssertionType type) {
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

static void failAssertMsg(AssertionType type, char const *msg) {
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

void yy::parser::error(std::string const &str) {
	::error("%s\n", str.c_str());
}
