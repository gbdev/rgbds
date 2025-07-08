// SPDX-License-Identifier: MIT

%language "c++"
%define api.value.type variant
%define api.token.constructor

%code requires {
	#include <stdint.h>
	#include <string>
	#include <variant>
	#include <vector>

	#include "linkdefs.hpp"

	#include "asm/lexer.hpp"
	#include "asm/macro.hpp"
	#include "asm/rpn.hpp"
	#include "asm/section.hpp"

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
	#include <algorithm>
	#include <ctype.h>
	#include <inttypes.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <string_view>

	#include "extern/utf8decoder.hpp"
	#include "helpers.hpp"

	#include "asm/charmap.hpp"
	#include "asm/fixpoint.hpp"
	#include "asm/format.hpp"
	#include "asm/fstack.hpp"
	#include "asm/main.hpp"
	#include "asm/opt.hpp"
	#include "asm/output.hpp"
	#include "asm/section.hpp"
	#include "asm/symbol.hpp"
	#include "asm/warning.hpp"

	using namespace std::literals;

	yy::parser::symbol_type yylex(); // Provided by lexer.cpp

	static uint32_t strToNum(std::vector<int32_t> const &s);
	static void errorInvalidUTF8Byte(uint8_t byte, char const *functionName);
	static size_t strlenUTF8(std::string const &str, bool printErrors);
	static std::string strsliceUTF8(std::string const &str, uint32_t start, uint32_t stop);
	static std::string strsubUTF8(std::string const &str, uint32_t pos, uint32_t len);
	static size_t charlenUTF8(std::string const &str);
	static std::string strcharUTF8(std::string const &str, uint32_t idx);
	static std::string charsubUTF8(std::string const &str, uint32_t pos);
	static int32_t charcmp(std::string_view str1, std::string_view str2);
	static uint32_t adjustNegativeIndex(int32_t idx, size_t len, char const *functionName);
	static uint32_t adjustNegativePos(int32_t pos, size_t len, char const *functionName);
	static std::string strrpl(std::string_view str, std::string const &old, std::string const &rep);
	static std::string strfmt(
	    std::string const &spec, std::vector<std::variant<uint32_t, std::string>> const &args
	);
	static void compoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue);
	static void failAssert(AssertionType type);
	static void failAssertMsg(AssertionType type, std::string const &message);

	template <typename N, typename S>
	static auto handleSymbolByType(std::string const &symName, N numCallback, S strCallback) {
		if (Symbol *sym = sym_FindScopedSymbol(symName); sym && sym->type == SYM_EQUS) {
			return strCallback(*sym->getEqus());
		} else {
			Expression expr;
			expr.makeSymbol(symName);
			return numCallback(expr);
		}
	}

	// The CPU encodes instructions in a logical way, so most instructions actually follow patterns.
	// These enums thus help with bit twiddling to compute opcodes.
	enum { REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_HL_IND, REG_A };

	enum { REG_BC_IND, REG_DE_IND, REG_HL_INDINC, REG_HL_INDDEC };

	// REG_AF == REG_SP since LD/INC/ADD/DEC allow SP, while PUSH/POP allow AF
	enum { REG_BC, REG_DE, REG_HL, REG_SP, REG_AF = REG_SP };
	// Names are not needed for AF or SP
	static char const *reg_tt_names[] = { "BC", "DE", "HL" };
	static char const *reg_tt_high_names[] = { "B", "D", "H" };
	static char const *reg_tt_low_names[] = { "C", "E", "L" };

	// CC_NZ == CC_Z ^ 1, and CC_NC == CC_C ^ 1, so `!` can toggle them
	enum { CC_NZ, CC_Z, CC_NC, CC_C };
}

/******************** Tokens ********************/

%token YYEOF 0 "end of file"
%token NEWLINE "end of line"
%token EOB "end of buffer"

// General punctuation
%token COMMA ","
%token COLON ":" DOUBLE_COLON "::"
%token LBRACK "[" RBRACK "]"
%token LPAREN "(" RPAREN ")"

// Arithmetic operators
%token OP_ADD "+" OP_SUB "-"
%token OP_MUL "*" OP_DIV "/" OP_MOD "%"
%token OP_EXP "**"

// String operators
%token OP_CAT "++"

// Comparison operators
%token OP_LOGICEQU "==" OP_LOGICNE "!="
%token OP_LOGICLT "<" OP_LOGICGT ">"
%token OP_LOGICLE "<=" OP_LOGICGE ">="

// Logical operators
%token OP_LOGICAND "&&" OP_LOGICOR "||"
%token OP_LOGICNOT "!"

// Binary operators
%token OP_AND "&" OP_OR "|" OP_XOR "^"
%token OP_SHL "<<" OP_SHR ">>" OP_USHR ">>>"
%token OP_NOT "~"

// Operator precedence
%left OP_LOGICOR
%left OP_LOGICAND
%left OP_LOGICEQU OP_LOGICNE OP_LOGICLT OP_LOGICGT OP_LOGICLE OP_LOGICGE
%left OP_ADD OP_SUB
%left OP_AND OP_OR OP_XOR
%left OP_SHL OP_SHR OP_USHR
%left OP_MUL OP_DIV OP_MOD
%left OP_CAT
%precedence NEG // applies to unary OP_LOGICNOT, OP_ADD, OP_SUB, OP_NOT
%right OP_EXP

// Assignment operators (only for variables)
%token POP_EQUAL "="
%token POP_ADDEQ "+=" POP_SUBEQ "-="
%token POP_MULEQ "*=" POP_DIVEQ "/=" POP_MODEQ "%="
%token POP_ANDEQ "&=" POP_OREQ "|=" POP_XOREQ "^="
%token POP_SHLEQ "<<=" POP_SHREQ ">>="

// SM83 registers
%token TOKEN_A "a"
%token TOKEN_B "b" TOKEN_C "c"
%token TOKEN_D "d" TOKEN_E "e"
%token TOKEN_H "h" TOKEN_L "l"
%token MODE_AF "af" MODE_BC "bc" MODE_DE "de" MODE_HL "hl" MODE_SP "sp"
%token MODE_HL_INC "hli/hl+" MODE_HL_DEC "hld/hl-"

// SM83 condition codes
%token CC_Z "z" CC_NZ "nz" CC_NC "nc" // There is no CC_C, only TOKEN_C

// SM83 instructions
%token SM83_ADC "adc"
%token SM83_ADD "add"
%token SM83_AND "and"
%token SM83_BIT "bit"
%token SM83_CALL "call"
%token SM83_CCF "ccf"
%token SM83_CP "cp"
%token SM83_CPL "cpl"
%token SM83_DAA "daa"
%token SM83_DEC "dec"
%token SM83_DI "di"
%token SM83_EI "ei"
%token SM83_HALT "halt"
%token SM83_INC "inc"
%token SM83_JP "jp"
%token SM83_JR "jr"
%token SM83_LDD "ldd"
%token SM83_LDH "ldh"
%token SM83_LDI "ldi"
%token SM83_LD "ld"
%token SM83_NOP "nop"
%token SM83_OR "or"
%token SM83_POP "pop"
%token SM83_PUSH "push"
%token SM83_RES "res"
%token SM83_RETI "reti"
%token SM83_RET "ret"
%token SM83_RLA "rla"
%token SM83_RLCA "rlca"
%token SM83_RLC "rlc"
%token SM83_RL "rl"
%token SM83_RRA "rra"
%token SM83_RRCA "rrca"
%token SM83_RRC "rrc"
%token SM83_RR "rr"
%token SM83_RST "rst"
%token SM83_SBC "sbc"
%token SM83_SCF "scf"
%token SM83_SET "set"
%token SM83_SLA "sla"
%token SM83_SRA "sra"
%token SM83_SRL "srl"
%token SM83_STOP "stop"
%token SM83_SUB "sub"
%token SM83_SWAP "swap"
%token SM83_XOR "xor"

// Statement keywords
%token POP_ALIGN "ALIGN"
%token POP_ASSERT "ASSERT"
%token POP_BREAK "BREAK"
%token POP_CHARMAP "CHARMAP"
%token POP_DB "DB"
%token POP_DL "DL"
%token POP_DS "DS"
%token POP_DW "DW"
%token POP_ELIF "ELIF"
%token POP_ELSE "ELSE"
%token POP_ENDC "ENDC"
%token POP_ENDL "ENDL"
%token POP_ENDM "ENDM"
%token POP_ENDR "ENDR"
%token POP_ENDSECTION "ENDSECTION"
%token POP_ENDU "ENDU"
%token POP_EQU "EQU"
%token POP_EQUS "EQUS"
%token POP_EXPORT "EXPORT"
%token POP_FAIL "FAIL"
%token POP_FATAL "FATAL"
%token POP_FOR "FOR"
%token POP_FRAGMENT "FRAGMENT"
%token POP_IF "IF"
%token POP_INCBIN "INCBIN"
%token POP_INCLUDE "INCLUDE"
%token POP_LOAD "LOAD"
%token POP_MACRO "MACRO"
%token POP_NEWCHARMAP "NEWCHARMAP"
%token POP_NEXTU "NEXTU"
%token POP_OPT "OPT"
%token POP_POPC "POPC"
%token POP_POPO "POPO"
%token POP_POPS "POPS"
%token POP_PRINTLN "PRINTLN"
%token POP_PRINT "PRINT"
%token POP_PURGE "PURGE"
%token POP_PUSHC "PUSHC"
%token POP_PUSHO "PUSHO"
%token POP_PUSHS "PUSHS"
%token POP_RB "RB"
%token POP_REDEF "REDEF"
%token POP_REPT "REPT"
%token POP_RSRESET "RSRESET"
%token POP_RSSET "RSSET"
// There is no POP_RL, only SM83_RL
%token POP_RW "RW"
%token POP_SECTION "SECTION"
%token POP_SETCHARMAP "SETCHARMAP"
%token POP_SHIFT "SHIFT"
%token POP_STATIC_ASSERT "STATIC_ASSERT"
%token POP_UNION "UNION"
%token POP_WARN "WARN"

// Function keywords
%token OP_ACOS "ACOS"
%token OP_ASIN "ASIN"
%token OP_ATAN "ATAN"
%token OP_ATAN2 "ATAN2"
%token OP_BANK "BANK"
%token OP_BITWIDTH "BITWIDTH"
%token OP_CEIL "CEIL"
%token OP_CHARCMP "CHARCMP"
%token OP_CHARLEN "CHARLEN"
%token OP_CHARSIZE "CHARSIZE"
%token OP_CHARSUB "CHARSUB"
%token OP_CHARVAL "CHARVAL"
%token OP_COS "COS"
%token OP_DEF "DEF"
%token OP_FDIV "FDIV"
%token OP_FLOOR "FLOOR"
%token OP_FMOD "FMOD"
%token OP_FMUL "FMUL"
%token OP_HIGH "HIGH"
%token OP_INCHARMAP "INCHARMAP"
%token OP_ISCONST "ISCONST"
%token OP_LOG "LOG"
%token OP_LOW "LOW"
%token OP_POW "POW"
%token OP_REVCHAR "REVCHAR"
%token OP_ROUND "ROUND"
%token OP_SIN "SIN"
%token OP_SIZEOF "SIZEOF"
%token OP_STARTOF "STARTOF"
%token OP_STRCAT "STRCAT"
%token OP_STRCHAR "STRCHAR"
%token OP_STRCMP "STRCMP"
%token OP_STRFIND "STRFIND"
%token OP_STRFMT "STRFMT"
%token OP_STRIN "STRIN"
%token OP_STRLEN "STRLEN"
%token OP_STRLWR "STRLWR"
%token OP_STRRFIND "STRRFIND"
%token OP_STRRIN "STRRIN"
%token OP_STRRPL "STRRPL"
%token OP_STRSLICE "STRSLICE"
%token OP_STRSUB "STRSUB"
%token OP_STRUPR "STRUPR"
%token OP_TAN "TAN"
%token OP_TZCOUNT "TZCOUNT"

// Section types
%token SECT_HRAM "HRAM"
%token SECT_OAM "OAM"
%token SECT_ROM0 "ROM0"
%token SECT_ROMX "ROMX"
%token SECT_SRAM "SRAM"
%token SECT_VRAM "VRAM"
%token SECT_WRAM0 "WRAM0"
%token SECT_WRAMX "WRAMX"

// Literals
%token <int32_t> NUMBER "number"
%token <std::string> STRING "string"
%token <std::string> SYMBOL "symbol"
%token <std::string> LABEL "label"
%token <std::string> LOCAL "local label"
%token <std::string> ANON "anonymous label"

/******************** Data types ********************/

// RPN expressions
%type <Expression> relocexpr
// `relocexpr_no_str` exists because strings usually count as numeric expressions, but some
// contexts treat numbers and strings differently, e.g. `db "string"` or `print "string"`.
%type <Expression> relocexpr_no_str
%type <Expression> reloc_3bit
%type <Expression> reloc_8bit
%type <Expression> reloc_8bit_offset
%type <Expression> reloc_16bit

// Constant numbers
%type <int32_t> iconst
%type <int32_t> uconst
// Constant numbers used only in specific contexts
%type <int32_t> precision_arg
%type <int32_t> rs_uconst
%type <int32_t> sect_org
%type <int32_t> shift_const

// Strings
%type <std::string> string
%type <std::string> string_literal
%type <std::string> strcat_args
// Strings used for identifiers
%type <std::string> def_id
%type <std::string> redef_id
%type <std::string> def_numeric
%type <std::string> def_equ
%type <std::string> redef_equ
%type <std::string> def_set
%type <std::string> def_rb
%type <std::string> def_rw
%type <std::string> def_rl
%type <std::string> def_equs
%type <std::string> redef_equs
%type <std::string> scoped_sym
// `scoped_sym_no_anon` exists because anonymous labels usually count as "scoped symbols", but some
// contexts treat anonymous labels and other labels/symbols differently, e.g. `purge` or `export`.
%type <std::string> scoped_sym_no_anon

// SM83 instruction parameters
%type <int32_t> reg_r
%type <int32_t> reg_r_no_a
%type <int32_t> reg_a
%type <int32_t> reg_ss
%type <int32_t> reg_rr
%type <int32_t> reg_tt
%type <int32_t> reg_tt_no_af
%type <int32_t> reg_bc_or_de
%type <int32_t> ccode_expr
%type <int32_t> ccode
%type <Expression> op_a_n
%type <int32_t> op_a_r
%type <Expression> op_mem_ind

// Data types used only in specific contexts
%type <AlignmentSpec> align_spec
%type <AssertionType> assert_type
%type <Capture> capture_macro
%type <Capture> capture_rept
%type <RPNCommand> compound_eq
%type <std::vector<int32_t>> charmap_args
%type <std::vector<Expression>> ds_args
%type <ForArgs> for_args
%type <std::shared_ptr<MacroArgs>> macro_args
%type <std::vector<std::string>> purge_args
%type <SectionSpec> sect_attrs
%type <SectionModifier> sect_mod
%type <SectionType> sect_type
%type <StrFmtArgList> strfmt_args
%type <StrFmtArgList> strfmt_va_args

%%

/******************** Parser rules ********************/

// Assembly files.

asm_file: lines;

lines:
	  %empty
	| lines diff_mark line
	// Continue parsing the next line on a syntax error
	| error {
		lexer_SetMode(LEXER_NORMAL);
		lexer_ToggleStringExpansion(true);
	} endofline {
		yyerrok;
	}
;

diff_mark:
	  %empty // OK
	| OP_ADD {
		::error(
		    "syntax error, unexpected + at the beginning of the line (is it a leftover diff mark?)"
		);
	}
	| OP_SUB {
		::error(
		    "syntax error, unexpected - at the beginning of the line (is it a leftover diff mark?)"
		);
	}
;

// Lines and line directives.

line:
	  plain_directive endofline
	| line_directive // Directives that manage newlines themselves
;

endofline: NEWLINE | EOB;

// For "logistical" reasons, these directives must manage newlines themselves.
// This is because we need to switch the lexer's mode *after* the newline has been read,
// and to avoid causing some grammar conflicts (token reducing is finicky).
// This is DEFINITELY one of the more FRAGILE parts of the codebase, handle with care.
line_directive:
	  def_macro
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
	POP_IF iconst NEWLINE {
		lexer_IncIFDepth();

		if ($2) {
			lexer_RunIFBlock();
		} else {
			lexer_SetMode(LEXER_SKIP_TO_ELIF);
		}
	}
;

elif:
	POP_ELIF iconst NEWLINE {
		if (lexer_GetIFDepth() == 0) {
			fatalerror("Found ELIF outside of an IF construct");
		}
		if (lexer_RanIFBlock()) {
			if (lexer_ReachedELSEBlock()) {
				fatalerror("Found ELIF after an ELSE block");
			}
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
		if (lexer_GetIFDepth() == 0) {
			fatalerror("Found ELSE outside of an IF construct");
		}
		if (lexer_RanIFBlock()) {
			if (lexer_ReachedELSEBlock()) {
				fatalerror("Found ELSE after an ELSE block");
			}
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
;

endc:
	POP_ENDC {
		lexer_DecIFDepth();
	}
;

def_id:
	OP_DEF {
		lexer_ToggleStringExpansion(false);
	} SYMBOL {
		lexer_ToggleStringExpansion(true);
		$$ = std::move($3);
	}
;

redef_id:
	POP_REDEF {
		lexer_ToggleStringExpansion(false);
	} SYMBOL {
		lexer_ToggleStringExpansion(true);
		$$ = std::move($3);
	}
;

scoped_sym_no_anon: SYMBOL | LABEL | LOCAL;

scoped_sym: scoped_sym_no_anon | ANON;

label:
	  %empty
	| LABEL COLON {
		sym_AddLabel($1);
	}
	| LABEL DOUBLE_COLON {
		sym_AddLabel($1);
		sym_Export($1);
	}
	| LOCAL {
		sym_AddLocalLabel($1);
	}
	| LOCAL COLON {
		sym_AddLocalLabel($1);
	}
	| LOCAL DOUBLE_COLON {
		sym_AddLocalLabel($1);
		sym_Export($1);
	}
	| COLON {
		sym_AddAnonLabel();
	}
;

macro:
	SYMBOL {
		// Parsing 'macro_args' will restore the lexer's normal mode
		lexer_SetMode(LEXER_RAW);
	} macro_args {
		fstk_RunMacro($1, $3);
	}
;

macro_args:
	%empty {
		$$ = std::make_shared<MacroArgs>();
	}
	| macro_args STRING {
		$$ = std::move($1);
		$$->appendArg(std::make_shared<std::string>($2));
	}
;

directive:
	  endc
	| print
	| println
	| export
	| export_def
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
	| pushc_setcharmap
	| load
	| shift
	| fail
	| warn
	| assert
	| def_numeric
	| def_equs
	| redef_equs
	| purge
	| pops
	| pushs
	| pushs_section
	| endsection
	| popo
	| pusho
	| opt
	| align
;

def_numeric:
	  def_equ
	| redef_equ
	| def_set
	| def_rb
	| def_rw
	| def_rl
;

trailing_comma: %empty | COMMA;

compound_eq:
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

align:
	POP_ALIGN align_spec {
		sect_AlignPC($2.alignment, $2.alignOfs);
	}
;

align_spec:
	uconst {
		if ($1 > 16) {
			::error("Alignment must be between 0 and 16, not %u", $1);
			$$.alignment = $$.alignOfs = 0;
		} else {
			$$.alignment = $1;
			$$.alignOfs = 0;
		}
	}
	| uconst COMMA iconst {
		if ($1 > 16) {
			::error("Alignment must be between 0 and 16, not %u", $1);
			$$.alignment = $$.alignOfs = 0;
		} else if ($3 <= -(1 << $1) || $3 >= 1 << $1) {
			::error(
			    "The absolute alignment offset (%" PRIu32 ") must be less than alignment size (%d)",
			    static_cast<uint32_t>($3 < 0 ? -$3 : $3),
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
		opt_Parse($1.c_str());
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
		// Parsing 'pusho_opt_list' will restore the lexer's normal mode
		lexer_SetMode(LEXER_RAW);
	} pusho_opt_list
;

pusho_opt_list:
	%empty {
		lexer_SetMode(LEXER_NORMAL);
	}
	| opt_list
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
		fatalerror("%s", $2.c_str());
	}
;

warn:
	POP_WARN string {
		warning(WARNING_USER, "%s", $2.c_str());
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
		if (!$3.isKnown()) {
			out_CreateAssert($2, $3, "", sect_GetOutputOffset());
		} else if ($3.value() == 0) {
			failAssert($2);
		}
	}
	| POP_ASSERT assert_type relocexpr COMMA string {
		if (!$3.isKnown()) {
			out_CreateAssert($2, $3, $5, sect_GetOutputOffset());
		} else if ($3.value() == 0) {
			failAssertMsg($2, $5);
		}
	}
	| POP_STATIC_ASSERT assert_type iconst {
		if ($3 == 0) {
			failAssert($2);
		}
	}
	| POP_STATIC_ASSERT assert_type iconst COMMA string {
		if ($3 == 0) {
			failAssertMsg($2, $5);
		}
	}
;

shift:
	POP_SHIFT shift_const {
		if (MacroArgs *macroArgs = fstk_GetCurrentMacroArgs(); macroArgs) {
			macroArgs->shiftArgs($2);
		} else {
			::error("Cannot shift macro arguments outside of a macro");
		}
	}
;

shift_const:
	%empty {
		$$ = 1;
	}
	| iconst
;

load:
	POP_LOAD sect_mod string COMMA sect_type sect_org sect_attrs {
		sect_SetLoadSection($3, $5, $6, $7, $2);
	}
	| POP_ENDL {
		sect_EndLoadSection(nullptr);
	}
;

rept:
	POP_REPT uconst NEWLINE capture_rept endofline {
		if ($4.span.ptr) {
			fstk_RunRept($2, $4.lineNo, $4.span);
		}
	}
;

for:
	POP_FOR {
		lexer_ToggleStringExpansion(false);
	} SYMBOL {
		lexer_ToggleStringExpansion(true);
	} COMMA for_args NEWLINE capture_rept endofline {
		if ($8.span.ptr) {
			fstk_RunFor($3, $6.start, $6.stop, $6.step, $8.lineNo, $8.span);
		}
	}
;

capture_rept:
	%empty {
		$$ = lexer_CaptureRept();
	}
;

for_args:
	iconst {
		$$.start = 0;
		$$.stop = $1;
		$$.step = 1;
	}
	| iconst COMMA iconst {
		$$.start = $1;
		$$.stop = $3;
		$$.step = 1;
	}
	| iconst COMMA iconst COMMA iconst {
		$$.start = $1;
		$$.stop = $3;
		$$.step = $5;
	}
;

break:
	label POP_BREAK endofline {
		if (fstk_Break()) {
			lexer_SetMode(LEXER_SKIP_TO_ENDR);
		}
	}
;

def_macro:
	POP_MACRO {
		lexer_ToggleStringExpansion(false);
	} SYMBOL {
		lexer_ToggleStringExpansion(true);
	} NEWLINE capture_macro endofline {
		if ($6.span.ptr) {
			sym_AddMacro($3, $6.lineNo, $6.span);
		}
	}
;

capture_macro:
	%empty {
		$$ = lexer_CaptureMacro();
	}
;

rsset:
	POP_RSSET uconst {
		sym_SetRSValue($2);
	}
;

rsreset:
	POP_RSRESET {
		sym_SetRSValue(0);
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
	| POP_DS POP_ALIGN LBRACK align_spec RBRACK trailing_comma {
		uint32_t n = sect_GetAlignBytes($4.alignment, $4.alignOfs);

		sect_Skip(n, true);
		sect_AlignPC($4.alignment, $4.alignOfs);
	}
	| POP_DS POP_ALIGN LBRACK align_spec RBRACK COMMA ds_args trailing_comma {
		uint32_t n = sect_GetAlignBytes($4.alignment, $4.alignOfs);

		sect_RelBytes(n, $7);
		sect_AlignPC($4.alignment, $4.alignOfs);
	}
;

ds_args:
	reloc_8bit {
		$$.push_back(std::move($1));
	}
	| ds_args COMMA reloc_8bit {
		$$ = std::move($1);
		$$.push_back(std::move($3));
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
	def_id POP_EQU iconst {
		$$ = std::move($1);
		sym_AddEqu($$, $3);
	}
;

redef_equ:
	redef_id POP_EQU iconst {
		$$ = std::move($1);
		sym_RedefEqu($$, $3);
	}
;

def_set:
	def_id POP_EQUAL iconst {
		$$ = std::move($1);
		sym_AddVar($$, $3);
	}
	| redef_id POP_EQUAL iconst {
		$$ = std::move($1);
		sym_AddVar($$, $3);
	}
	| def_id compound_eq iconst {
		$$ = std::move($1);
		compoundAssignment($$, $2, $3);
	}
	| redef_id compound_eq iconst {
		$$ = std::move($1);
		compoundAssignment($$, $2, $3);
	}
;

def_rb:
	def_id POP_RB rs_uconst {
		$$ = std::move($1);
		uint32_t rs = sym_GetRSValue();
		sym_AddEqu($$, rs);
		sym_SetRSValue(rs + $3);
	}
;

def_rw:
	def_id POP_RW rs_uconst {
		$$ = std::move($1);
		uint32_t rs = sym_GetRSValue();
		sym_AddEqu($$, rs);
		sym_SetRSValue(rs + 2 * $3);
	}
;

def_rl:
	def_id SM83_RL rs_uconst {
		$$ = std::move($1);
		uint32_t rs = sym_GetRSValue();
		sym_AddEqu($$, rs);
		sym_SetRSValue(rs + 4 * $3);
	}
;

def_equs:
	def_id POP_EQUS string {
		$$ = std::move($1);
		sym_AddString($$, std::make_shared<std::string>($3));
	}
;

redef_equs:
	redef_id POP_EQUS string {
		$$ = std::move($1);
		sym_RedefString($$, std::make_shared<std::string>($3));
	}
;

purge:
	POP_PURGE {
		lexer_ToggleStringExpansion(false);
	} purge_args trailing_comma {
		for (std::string &arg : $3) {
			sym_Purge(arg);
		}
		lexer_ToggleStringExpansion(true);
	}
;

purge_args:
	scoped_sym_no_anon {
		$$.push_back($1);
	}
	| purge_args COMMA scoped_sym_no_anon {
		$$ = std::move($1);
		$$.push_back($3);
	}
;

export: POP_EXPORT export_list trailing_comma;

export_list:
	  export_list_entry
	| export_list COMMA export_list_entry
;

export_list_entry:
	scoped_sym_no_anon {
		sym_Export($1);
	}
;

export_def:
	POP_EXPORT def_numeric {
		sym_Export($2);
	}
;

include:
	label POP_INCLUDE string endofline {
		fstk_RunInclude($3, false);
		if (failedOnMissingInclude) {
			YYACCEPT;
		}
	}
;

incbin:
	POP_INCBIN string {
		sect_BinaryFile($2, 0);
		if (failedOnMissingInclude) {
			YYACCEPT;
		}
	}
	| POP_INCBIN string COMMA iconst {
		sect_BinaryFile($2, $4);
		if (failedOnMissingInclude) {
			YYACCEPT;
		}
	}
	| POP_INCBIN string COMMA iconst COMMA iconst {
		sect_BinaryFileSlice($2, $4, $6);
		if (failedOnMissingInclude) {
			YYACCEPT;
		}
	}
;

charmap:
	POP_CHARMAP string COMMA charmap_args trailing_comma {
		charmap_Add($2, std::move($4));
	}
;

charmap_args:
	iconst {
		$$.push_back(std::move($1));
	}
	| charmap_args COMMA iconst {
		$$ = std::move($1);
		$$.push_back(std::move($3));
	}
;

newcharmap:
	POP_NEWCHARMAP SYMBOL {
		charmap_New($2, nullptr);
	}
	| POP_NEWCHARMAP SYMBOL COMMA SYMBOL {
		charmap_New($2, &$4);
	}
;

setcharmap:
	POP_SETCHARMAP SYMBOL {
		charmap_Set($2);
	}
;

pushc:
	POP_PUSHC {
		charmap_Push();
	}
;

pushc_setcharmap:
	POP_PUSHC SYMBOL {
		charmap_Push();
		charmap_Set($2);
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
	relocexpr_no_str {
		printf("$%" PRIX32, $1.getConstVal());
	}
	| string_literal {
		// Allow printing NUL characters
		fwrite($1.data(), 1, $1.length(), stdout);
	}
	| scoped_sym {
		handleSymbolByType(
		    $1,
		    [](Expression const &expr) { printf("$%" PRIX32, expr.getConstVal()); },
		    [](std::string const &str) { fwrite(str.data(), 1, str.length(), stdout); }
		);
	}
;

reloc_3bit:
	relocexpr {
		$$ = std::move($1);
		$$.checkNBit(3);
	}
;

constlist_8bit:
	  constlist_8bit_entry
	| constlist_8bit COMMA constlist_8bit_entry
;

constlist_8bit_entry:
	relocexpr_no_str {
		$1.checkNBit(8);
		sect_RelByte($1, 0);
	}
	| string_literal {
		std::vector<int32_t> output = charmap_Convert($1);
		sect_ByteString(output);
	}
	| scoped_sym {
		handleSymbolByType(
		    $1,
		    [](Expression const &expr) {
			    expr.checkNBit(8);
			    sect_RelByte(expr, 0);
		    },
		    [](std::string const &str) {
			    std::vector<int32_t> output = charmap_Convert(str);
			    sect_ByteString(output);
		    }
		);
	}
;

constlist_16bit:
	  constlist_16bit_entry
	| constlist_16bit COMMA constlist_16bit_entry
;

constlist_16bit_entry:
	relocexpr_no_str {
		$1.checkNBit(16);
		sect_RelWord($1, 0);
	}
	| string_literal {
		std::vector<int32_t> output = charmap_Convert($1);
		sect_WordString(output);
	}
	| scoped_sym {
		handleSymbolByType(
		    $1,
		    [](Expression const &expr) {
			    expr.checkNBit(16);
			    sect_RelWord(expr, 0);
		    },
		    [](std::string const &str) {
			    std::vector<int32_t> output = charmap_Convert(str);
			    sect_WordString(output);
		    }
		);
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
	| string_literal {
		std::vector<int32_t> output = charmap_Convert($1);
		sect_LongString(output);
	}
	| scoped_sym {
		handleSymbolByType(
		    $1,
		    [](Expression const &expr) { sect_RelLong(expr, 0); },
		    [](std::string const &str) {
			    std::vector<int32_t> output = charmap_Convert(str);
			    sect_LongString(output);
		    }
		);
	}
;

reloc_8bit:
	relocexpr {
		$$ = std::move($1);
		$$.checkNBit(8);
	}
;

reloc_8bit_offset:
	OP_ADD relocexpr {
		$$ = std::move($2);
		$$.checkNBit(8);
	}
	| OP_SUB relocexpr {
		$$.makeUnaryOp(RPN_NEG, std::move($2));
		$$.checkNBit(8);
	}
;

reloc_16bit:
	relocexpr {
		$$ = std::move($1);
		$$.checkNBit(16);
	}
;

relocexpr:
	relocexpr_no_str {
		$$ = std::move($1);
	}
	| string_literal {
		std::vector<int32_t> output = charmap_Convert($1);
		$$.makeNumber(strToNum(output));
	}
	| scoped_sym {
		$$ = handleSymbolByType(
		    $1,
		    [](Expression const &expr) { return expr; },
		    [](std::string const &str) {
			    std::vector<int32_t> output = charmap_Convert(str);
			    Expression expr;
			    expr.makeNumber(strToNum(output));
			    return expr;
		    }
		);
	}
;

relocexpr_no_str:
	NUMBER {
		$$.makeNumber($1);
	}
	| OP_LOGICNOT relocexpr %prec NEG {
		$$.makeUnaryOp(RPN_LOGNOT, std::move($2));
	}
	| relocexpr OP_LOGICOR relocexpr {
		$$.makeBinaryOp(RPN_LOGOR, std::move($1), $3);
	}
	| relocexpr OP_LOGICAND relocexpr {
		$$.makeBinaryOp(RPN_LOGAND, std::move($1), $3);
	}
	| relocexpr OP_LOGICEQU relocexpr {
		$$.makeBinaryOp(RPN_LOGEQ, std::move($1), $3);
	}
	| relocexpr OP_LOGICGT relocexpr {
		$$.makeBinaryOp(RPN_LOGGT, std::move($1), $3);
	}
	| relocexpr OP_LOGICLT relocexpr {
		$$.makeBinaryOp(RPN_LOGLT, std::move($1), $3);
	}
	| relocexpr OP_LOGICGE relocexpr {
		$$.makeBinaryOp(RPN_LOGGE, std::move($1), $3);
	}
	| relocexpr OP_LOGICLE relocexpr {
		$$.makeBinaryOp(RPN_LOGLE, std::move($1), $3);
	}
	| relocexpr OP_LOGICNE relocexpr {
		$$.makeBinaryOp(RPN_LOGNE, std::move($1), $3);
	}
	| relocexpr OP_ADD relocexpr {
		$$.makeBinaryOp(RPN_ADD, std::move($1), $3);
	}
	| relocexpr OP_SUB relocexpr {
		$$.makeBinaryOp(RPN_SUB, std::move($1), $3);
	}
	| relocexpr OP_XOR relocexpr {
		$$.makeBinaryOp(RPN_XOR, std::move($1), $3);
	}
	| relocexpr OP_OR relocexpr {
		$$.makeBinaryOp(RPN_OR, std::move($1), $3);
	}
	| relocexpr OP_AND relocexpr {
		$$.makeBinaryOp(RPN_AND, std::move($1), $3);
	}
	| relocexpr OP_SHL relocexpr {
		$$.makeBinaryOp(RPN_SHL, std::move($1), $3);
	}
	| relocexpr OP_SHR relocexpr {
		$$.makeBinaryOp(RPN_SHR, std::move($1), $3);
	}
	| relocexpr OP_USHR relocexpr {
		$$.makeBinaryOp(RPN_USHR, std::move($1), $3);
	}
	| relocexpr OP_MUL relocexpr {
		$$.makeBinaryOp(RPN_MUL, std::move($1), $3);
	}
	| relocexpr OP_DIV relocexpr {
		$$.makeBinaryOp(RPN_DIV, std::move($1), $3);
	}
	| relocexpr OP_MOD relocexpr {
		$$.makeBinaryOp(RPN_MOD, std::move($1), $3);
	}
	| relocexpr OP_EXP relocexpr {
		$$.makeBinaryOp(RPN_EXP, std::move($1), $3);
	}
	| OP_ADD relocexpr %prec NEG {
		$$ = std::move($2);
	}
	| OP_SUB relocexpr %prec NEG {
		$$.makeUnaryOp(RPN_NEG, std::move($2));
	}
	| OP_NOT relocexpr %prec NEG {
		$$.makeUnaryOp(RPN_NOT, std::move($2));
	}
	| OP_HIGH LPAREN relocexpr RPAREN {
		$$.makeUnaryOp(RPN_HIGH, std::move($3));
	}
	| OP_LOW LPAREN relocexpr RPAREN {
		$$.makeUnaryOp(RPN_LOW, std::move($3));
	}
	| OP_BITWIDTH LPAREN relocexpr RPAREN {
		$$.makeUnaryOp(RPN_BITWIDTH, std::move($3));
	}
	| OP_TZCOUNT LPAREN relocexpr RPAREN {
		$$.makeUnaryOp(RPN_TZCOUNT, std::move($3));
	}
	| OP_ISCONST LPAREN relocexpr RPAREN {
		$$.makeNumber($3.isKnown());
	}
	| OP_BANK LPAREN scoped_sym RPAREN {
		// '@' is also a SYMBOL; it is handled here
		$$.makeBankSymbol($3);
	}
	| OP_BANK LPAREN string_literal RPAREN {
		$$.makeBankSection($3);
	}
	| OP_SIZEOF LPAREN string RPAREN {
		$$.makeSizeOfSection($3);
	}
	| OP_STARTOF LPAREN string RPAREN {
		$$.makeStartOfSection($3);
	}
	| OP_SIZEOF LPAREN sect_type RPAREN {
		$$.makeSizeOfSectionType($3);
	}
	| OP_STARTOF LPAREN sect_type RPAREN {
		$$.makeStartOfSectionType($3);
	}
	| OP_DEF {
		lexer_ToggleStringExpansion(false);
	} LPAREN scoped_sym RPAREN {
		$$.makeNumber(sym_FindScopedValidSymbol($4) != nullptr);
		lexer_ToggleStringExpansion(true);
	}
	| OP_ROUND LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Round($3, $4));
	}
	| OP_CEIL LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Ceil($3, $4));
	}
	| OP_FLOOR LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Floor($3, $4));
	}
	| OP_FDIV LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_Div($3, $5, $6));
	}
	| OP_FMUL LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_Mul($3, $5, $6));
	}
	| OP_FMOD LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_Mod($3, $5, $6));
	}
	| OP_POW LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_Pow($3, $5, $6));
	}
	| OP_LOG LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_Log($3, $5, $6));
	}
	| OP_SIN LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Sin($3, $4));
	}
	| OP_COS LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Cos($3, $4));
	}
	| OP_TAN LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_Tan($3, $4));
	}
	| OP_ASIN LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_ASin($3, $4));
	}
	| OP_ACOS LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_ACos($3, $4));
	}
	| OP_ATAN LPAREN iconst precision_arg RPAREN {
		$$.makeNumber(fix_ATan($3, $4));
	}
	| OP_ATAN2 LPAREN iconst COMMA iconst precision_arg RPAREN {
		$$.makeNumber(fix_ATan2($3, $5, $6));
	}
	| OP_STRCMP LPAREN string COMMA string RPAREN {
		$$.makeNumber($3.compare($5));
	}
	| OP_STRFIND LPAREN string COMMA string RPAREN {
		size_t pos = $3.find($5);
		$$.makeNumber(pos != std::string::npos ? pos : -1);
	}
	| OP_STRRFIND LPAREN string COMMA string RPAREN {
		size_t pos = $3.rfind($5);
		$$.makeNumber(pos != std::string::npos ? pos : -1);
	}
	| OP_STRIN LPAREN string COMMA string RPAREN {
		size_t pos = $3.find($5);
		$$.makeNumber(pos != std::string::npos ? pos + 1 : 0);
	}
	| OP_STRRIN LPAREN string COMMA string RPAREN {
		size_t pos = $3.rfind($5);
		$$.makeNumber(pos != std::string::npos ? pos + 1 : 0);
	}
	| OP_STRLEN LPAREN string RPAREN {
		$$.makeNumber(strlenUTF8($3, true));
	}
	| OP_CHARLEN LPAREN string RPAREN {
		$$.makeNumber(charlenUTF8($3));
	}
	| OP_INCHARMAP LPAREN string RPAREN {
		$$.makeNumber(charmap_HasChar($3));
	}
	| OP_CHARCMP LPAREN string COMMA string RPAREN {
		$$.makeNumber(charcmp($3, $5));
	}
	| OP_CHARSIZE LPAREN string RPAREN {
		size_t charSize = charmap_CharSize($3);
		if (charSize == 0) {
			::error("CHARSIZE: No character mapping for \"%s\"", $3.c_str());
		}
		$$.makeNumber(charSize);
	}
	| OP_CHARVAL LPAREN string COMMA iconst RPAREN {
		if (size_t len = charmap_CharSize($3); len != 0) {
			uint32_t idx = adjustNegativeIndex($5, len, "CHARVAL");
			if (std::optional<int32_t> val = charmap_CharValue($3, idx); val.has_value()) {
				$$.makeNumber(*val);
			} else {
				warning(
				    WARNING_BUILTIN_ARG,
				    "CHARVAL: Index %" PRIu32 " is past the end of the character mapping",
				    idx
				);
				$$.makeNumber(0);
			}
		} else {
			::error("CHARVAL: No character mapping for \"%s\"", $3.c_str());
			$$.makeNumber(0);
		}
	}
	| LPAREN relocexpr RPAREN {
		$$ = std::move($2);
	}
;

uconst:
	iconst {
		$$ = $1;
		if ($$ < 0) {
			fatalerror("Constant must not be negative: %d", $$);
		}
	}
;

iconst:
	relocexpr {
		$$ = $1.getConstVal();
	}
;

precision_arg:
	%empty {
		$$ = fix_Precision();
	}
	| COMMA iconst {
		$$ = $2;
		if ($$ < 1 || $$ > 31) {
			::error("Fixed-point precision must be between 1 and 31, not %" PRId32, $$);
			$$ = fix_Precision();
		}
	}
;

string_literal:
	STRING {
		$$ = std::move($1);
	}
	| string OP_CAT string {
		$$ = std::move($1);
		$$.append($3);
	}
	| OP_STRSLICE LPAREN string COMMA iconst COMMA iconst RPAREN {
		size_t len = strlenUTF8($3, false);
		uint32_t start = adjustNegativeIndex($5, len, "STRSLICE");
		uint32_t stop = adjustNegativeIndex($7, len, "STRSLICE");
		$$ = strsliceUTF8($3, start, stop);
	}
	| OP_STRSLICE LPAREN string COMMA iconst RPAREN {
		size_t len = strlenUTF8($3, false);
		uint32_t start = adjustNegativeIndex($5, len, "STRSLICE");
		$$ = strsliceUTF8($3, start, len);
	}
	| OP_STRSUB LPAREN string COMMA iconst COMMA uconst RPAREN {
		size_t len = strlenUTF8($3, false);
		uint32_t pos = adjustNegativePos($5, len, "STRSUB");
		$$ = strsubUTF8($3, pos, $7);
	}
	| OP_STRSUB LPAREN string COMMA iconst RPAREN {
		size_t len = strlenUTF8($3, false);
		uint32_t pos = adjustNegativePos($5, len, "STRSUB");
		$$ = strsubUTF8($3, pos, pos > len ? 0 : len + 1 - pos);
	}
	| OP_STRCHAR LPAREN string COMMA iconst RPAREN {
		size_t len = charlenUTF8($3);
		uint32_t idx = adjustNegativeIndex($5, len, "STRCHAR");
		$$ = strcharUTF8($3, idx);
	}
	| OP_CHARSUB LPAREN string COMMA iconst RPAREN {
		size_t len = charlenUTF8($3);
		uint32_t pos = adjustNegativePos($5, len, "CHARSUB");
		$$ = charsubUTF8($3, pos);
	}
	| OP_REVCHAR LPAREN charmap_args RPAREN {
		bool unique;
		$$ = charmap_Reverse($3, unique);
		if (!unique) {
			::error("REVCHAR: Multiple character mappings to values");
		} else if ($$.empty()) {
			::error("REVCHAR: No character mapping to values");
		}
	}
	| OP_STRCAT LPAREN RPAREN {
		$$.clear();
	}
	| OP_STRCAT LPAREN strcat_args RPAREN {
		$$ = std::move($3);
	}
	| OP_STRUPR LPAREN string RPAREN {
		$$ = std::move($3);
		std::transform(RANGE($$), $$.begin(), [](char c) { return toupper(c); });
	}
	| OP_STRLWR LPAREN string RPAREN {
		$$ = std::move($3);
		std::transform(RANGE($$), $$.begin(), [](char c) { return tolower(c); });
	}
	| OP_STRRPL LPAREN string COMMA string COMMA string RPAREN {
		$$ = strrpl($3, $5, $7);
	}
	| OP_STRFMT LPAREN strfmt_args RPAREN {
		$$ = strfmt($3.format, $3.args);
	}
	| POP_SECTION LPAREN scoped_sym RPAREN {
		Symbol *sym = sym_FindScopedValidSymbol($3);

		if (!sym) {
			if (sym_IsPurgedScoped($3)) {
				fatalerror("Unknown symbol \"%s\"; it was purged", $3.c_str());
			} else {
				fatalerror("Unknown symbol \"%s\"", $3.c_str());
			}
		}
		Section const *section = sym->getSection();

		if (!section) {
			fatalerror("\"%s\" does not belong to any section", sym->name.c_str());
		}
		// Section names are capped by rgbasm's maximum string length,
		// so this currently can't overflow.
		$$ = section->name;
	}
;

string:
	string_literal {
		$$ = std::move($1);
	}
	| scoped_sym {
		if (Symbol *sym = sym_FindScopedSymbol($1); sym && sym->type == SYM_EQUS) {
			$$ = *sym->getEqus();
		} else {
			::error("'%s' is not a string symbol", $1.c_str());
		}
	}
;

strcat_args:
	string {
		$$ = std::move($1);
	}
	| strcat_args COMMA string {
		$$ = std::move($1);
		$$.append($3);
	}
;

strfmt_args:
	  %empty {}
	| string strfmt_va_args {
		$$ = std::move($2);
		$$.format = std::move($1);
	}
;

strfmt_va_args:
	  %empty {}
	| strfmt_va_args COMMA relocexpr_no_str {
		$$ = std::move($1);
		$$.args.push_back(static_cast<uint32_t>($3.getConstVal()));
	}
	| strfmt_va_args COMMA string_literal {
		$$ = std::move($1);
		$$.args.push_back(std::move($3));
	}
	| strfmt_va_args COMMA scoped_sym {
		$$ = std::move($1);
		handleSymbolByType(
		    $3,
		    [&](Expression const &expr) {
			    $$.args.push_back(static_cast<uint32_t>(expr.getConstVal()));
		    },
		    [&](std::string const &str) { $$.args.push_back(str); }
		);
	}
;

section:
	POP_SECTION sect_mod string COMMA sect_type sect_org sect_attrs {
		sect_NewSection($3, $5, $6, $7, $2);
	}
;

pushs_section:
	POP_PUSHS sect_mod string COMMA sect_type sect_org sect_attrs {
		sect_PushSection();
		sect_NewSection($3, $5, $6, $7, $2);
	}
;

sect_mod:
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

sect_type:
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

sect_org:
	%empty {
		$$ = -1;
	}
	| LBRACK uconst RBRACK {
		$$ = $2;
		if ($$ < 0 || $$ > 0xFFFF) {
			::error("Address $%x is not 16-bit", $$);
			$$ = -1;
		}
	}
;

sect_attrs:
	%empty {
		$$.alignment = 0;
		$$.alignOfs = 0;
		$$.bank = -1;
	}
	| sect_attrs COMMA POP_ALIGN LBRACK align_spec RBRACK {
		$$ = $1;
		$$.alignment = $5.alignment;
		$$.alignOfs = $5.alignOfs;
	}
	| sect_attrs COMMA OP_BANK LBRACK uconst RBRACK {
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
	  sm83_adc
	| sm83_add
	| sm83_and
	| sm83_bit
	| sm83_call
	| sm83_ccf
	| sm83_cp
	| sm83_cpl
	| sm83_daa
	| sm83_dec
	| sm83_di
	| sm83_ei
	| sm83_halt
	| sm83_inc
	| sm83_jp
	| sm83_jr
	| sm83_ld
	| sm83_ldd
	| sm83_ldh
	| sm83_ldi
	| sm83_nop
	| sm83_or
	| sm83_pop
	| sm83_push
	| sm83_res
	| sm83_ret
	| sm83_reti
	| sm83_rl
	| sm83_rla
	| sm83_rlc
	| sm83_rlca
	| sm83_rr
	| sm83_rra
	| sm83_rrc
	| sm83_rrca
	| sm83_rst
	| sm83_sbc
	| sm83_scf
	| sm83_set
	| sm83_sla
	| sm83_sra
	| sm83_srl
	| sm83_stop
	| sm83_sub
	| sm83_swap
	| sm83_xor
;

sm83_adc:
	SM83_ADC op_a_n {
		sect_ConstByte(0xCE);
		sect_RelByte($2, 1);
	}
	| SM83_ADC op_a_r {
		sect_ConstByte(0x88 | $2);
	}
;

sm83_add:
	SM83_ADD op_a_n {
		sect_ConstByte(0xC6);
		sect_RelByte($2, 1);
	}
	| SM83_ADD op_a_r {
		sect_ConstByte(0x80 | $2);
	}
	| SM83_ADD MODE_HL COMMA reg_ss {
		sect_ConstByte(0x09 | ($4 << 4));
	}
	| SM83_ADD MODE_SP COMMA reloc_8bit {
		sect_ConstByte(0xE8);
		sect_RelByte($4, 1);
	}
;

sm83_and:
	SM83_AND op_a_n {
		sect_ConstByte(0xE6);
		sect_RelByte($2, 1);
	}
	| SM83_AND op_a_r {
		sect_ConstByte(0xA0 | $2);
	}
;

sm83_bit:
	SM83_BIT reloc_3bit COMMA reg_r {
		uint8_t mask = static_cast<uint8_t>(0x40 | $4);
		$2.makeCheckBitIndex(mask);
		sect_ConstByte(0xCB);
		if (!$2.isKnown()) {
			sect_RelByte($2, 0);
		} else {
			sect_ConstByte(mask | ($2.value() << 3));
		}
	}
;

sm83_call:
	SM83_CALL reloc_16bit {
		sect_ConstByte(0xCD);
		sect_RelWord($2, 1);
	}
	| SM83_CALL ccode_expr COMMA reloc_16bit {
		sect_ConstByte(0xC4 | ($2 << 3));
		sect_RelWord($4, 1);
	}
;

sm83_ccf:
	SM83_CCF {
		sect_ConstByte(0x3F);
	}
;

sm83_cp:
	SM83_CP op_a_n {
		sect_ConstByte(0xFE);
		sect_RelByte($2, 1);
	}
	| SM83_CP op_a_r {
		sect_ConstByte(0xB8 | $2);
	}
;

sm83_cpl:
	SM83_CPL {
		sect_ConstByte(0x2F);
	}
	| SM83_CPL MODE_A {
		sect_ConstByte(0x2F);
	}
;

sm83_daa:
	SM83_DAA {
		sect_ConstByte(0x27);
	}
;

sm83_dec:
	SM83_DEC reg_r {
		sect_ConstByte(0x05 | ($2 << 3));
	}
	| SM83_DEC reg_ss {
		sect_ConstByte(0x0B | ($2 << 4));
	}
;

sm83_di:
	SM83_DI {
		sect_ConstByte(0xF3);
	}
;

sm83_ei:
	SM83_EI {
		sect_ConstByte(0xFB);
	}
;

sm83_halt:
	SM83_HALT {
		sect_ConstByte(0x76);
	}
;

sm83_inc:
	SM83_INC reg_r {
		sect_ConstByte(0x04 | ($2 << 3));
	}
	| SM83_INC reg_ss {
		sect_ConstByte(0x03 | ($2 << 4));
	}
;

sm83_jp:
	SM83_JP reloc_16bit {
		sect_ConstByte(0xC3);
		sect_RelWord($2, 1);
	}
	| SM83_JP ccode_expr COMMA reloc_16bit {
		sect_ConstByte(0xC2 | ($2 << 3));
		sect_RelWord($4, 1);
	}
	| SM83_JP MODE_HL {
		sect_ConstByte(0xE9);
	}
;

sm83_jr:
	SM83_JR reloc_16bit {
		sect_ConstByte(0x18);
		sect_PCRelByte($2, 1);
	}
	| SM83_JR ccode_expr COMMA reloc_16bit {
		sect_ConstByte(0x20 | ($2 << 3));
		sect_PCRelByte($4, 1);
	}
;

sm83_ldi:
	SM83_LDI LBRACK MODE_HL RBRACK COMMA MODE_A {
		sect_ConstByte(0x02 | (2 << 4));
	}
	| SM83_LDI MODE_A COMMA LBRACK MODE_HL RBRACK {
		sect_ConstByte(0x0A | (2 << 4));
	}
;

sm83_ldd:
	SM83_LDD LBRACK MODE_HL RBRACK COMMA MODE_A {
		sect_ConstByte(0x02 | (3 << 4));
	}
	| SM83_LDD MODE_A COMMA LBRACK MODE_HL RBRACK {
		sect_ConstByte(0x0A | (3 << 4));
	}
;

sm83_ldh:
	SM83_LDH MODE_A COMMA op_mem_ind {
		if ($4.makeCheckHRAM()) {
			warning(
			    WARNING_OBSOLETE,
			    "LDH is deprecated with values from $00 to $FF; use $FF00 to $FFFF"
			);
		}

		sect_ConstByte(0xF0);
		sect_RelByte($4, 1);
	}
	| SM83_LDH op_mem_ind COMMA MODE_A {
		if ($2.makeCheckHRAM()) {
			warning(
			    WARNING_OBSOLETE,
			    "LDH is deprecated with values from $00 to $FF; use $FF00 to $FFFF"
			);
		}

		sect_ConstByte(0xE0);
		sect_RelByte($2, 1);
	}
	| SM83_LDH MODE_A COMMA c_ind {
		sect_ConstByte(0xF2);
	}
	| SM83_LDH MODE_A COMMA ff00_c_ind {
		sect_ConstByte(0xF2);
	}
	| SM83_LDH c_ind COMMA MODE_A {
		sect_ConstByte(0xE2);
	}
	| SM83_LDH ff00_c_ind COMMA MODE_A {
		sect_ConstByte(0xE2);
	}
;

c_ind: LBRACK MODE_C RBRACK;

ff00_c_ind:
	LBRACK relocexpr OP_ADD MODE_C RBRACK {
		// This has to use `relocexpr`, not `iconst`, to avoid a shift/reduce conflict
		if ($2.getConstVal() != 0xFF00) {
			::error("Base value must be equal to $FF00 for $FF00+C");
		}
	}
;

sm83_ld:
	  sm83_ld_mem
	| sm83_ld_c_ind
	| sm83_ld_rr
	| sm83_ld_ss
	| sm83_ld_hl
	| sm83_ld_sp
	| sm83_ld_r_no_a
	| sm83_ld_a
;

sm83_ld_hl:
	SM83_LD MODE_HL COMMA MODE_SP reloc_8bit_offset {
		sect_ConstByte(0xF8);
		sect_RelByte($5, 1);
	}
	| SM83_LD MODE_HL COMMA MODE_SP {
		::error("LD HL, SP is not a valid instruction; use LD HL, SP + 0");
	}
	| SM83_LD MODE_HL COMMA reloc_16bit {
		sect_ConstByte(0x01 | (REG_HL << 4));
		sect_RelWord($4, 1);
	}
	| SM83_LD MODE_HL COMMA reg_tt_no_af {
		::error(
		    "LD HL, %s is not a valid instruction; use LD H, %s and LD L, %s",
		    reg_tt_names[$4],
		    reg_tt_high_names[$4],
		    reg_tt_low_names[$4]
		);
	}
;

sm83_ld_sp:
	SM83_LD MODE_SP COMMA MODE_HL {
		sect_ConstByte(0xF9);
	}
	| SM83_LD MODE_SP COMMA reg_bc_or_de {
		::error("LD SP, %s is not a valid instruction", reg_tt_names[$4]);
	}
	| SM83_LD MODE_SP COMMA reloc_16bit {
		sect_ConstByte(0x01 | (REG_SP << 4));
		sect_RelWord($4, 1);
	}
;

sm83_ld_mem:
	SM83_LD op_mem_ind COMMA MODE_SP {
		sect_ConstByte(0x08);
		sect_RelWord($2, 1);
	}
	| SM83_LD op_mem_ind COMMA MODE_A {
		sect_ConstByte(0xEA);
		sect_RelWord($2, 1);
	}
;

sm83_ld_c_ind:
	SM83_LD ff00_c_ind COMMA MODE_A {
		sect_ConstByte(0xE2);
	}
	| SM83_LD c_ind COMMA MODE_A {
		warning(WARNING_OBSOLETE, "LD [C], A is deprecated; use LDH [C], A");
		sect_ConstByte(0xE2);
	}
;

sm83_ld_rr:
	SM83_LD reg_rr COMMA MODE_A {
		sect_ConstByte(0x02 | ($2 << 4));
	}
;

sm83_ld_r_no_a:
	SM83_LD reg_r_no_a COMMA reloc_8bit {
		sect_ConstByte(0x06 | ($2 << 3));
		sect_RelByte($4, 1);
	}
	| SM83_LD reg_r_no_a COMMA reg_r {
		if ($2 == REG_HL_IND && $4 == REG_HL_IND) {
			::error("LD [HL], [HL] is not a valid instruction");
		} else {
			sect_ConstByte(0x40 | ($2 << 3) | $4);
		}
	}
;

sm83_ld_a:
	SM83_LD reg_a COMMA reloc_8bit {
		sect_ConstByte(0x06 | ($2 << 3));
		sect_RelByte($4, 1);
	}
	| SM83_LD reg_a COMMA reg_r {
		sect_ConstByte(0x40 | ($2 << 3) | $4);
	}
	| SM83_LD reg_a COMMA ff00_c_ind {
		sect_ConstByte(0xF2);
	}
	| SM83_LD reg_a COMMA c_ind {
		warning(WARNING_OBSOLETE, "LD A, [C] is deprecated; use LDH A, [C]");
		sect_ConstByte(0xF2);
	}
	| SM83_LD reg_a COMMA reg_rr {
		sect_ConstByte(0x0A | ($4 << 4));
	}
	| SM83_LD reg_a COMMA op_mem_ind {
		sect_ConstByte(0xFA);
		sect_RelWord($4, 1);
	}
;

sm83_ld_ss:
	SM83_LD reg_bc_or_de COMMA reloc_16bit {
		sect_ConstByte(0x01 | ($2 << 4));
		sect_RelWord($4, 1);
	}
	| SM83_LD reg_bc_or_de COMMA reg_tt_no_af {
		::error(
		    "LD %s, %s is not a valid instruction; use LD %s, %s and LD %s, %s",
		    reg_tt_names[$2],
		    reg_tt_names[$4],
		    reg_tt_high_names[$2],
		    reg_tt_high_names[$4],
		    reg_tt_low_names[$2],
		    reg_tt_low_names[$4]
		);
	}
	// HL is taken care of in sm83_ld_hl
	// SP is taken care of in sm83_ld_sp
;

sm83_nop:
	SM83_NOP {
		sect_ConstByte(0x00);
	}
;

sm83_or:
	SM83_OR op_a_n {
		sect_ConstByte(0xF6);
		sect_RelByte($2, 1);
	}
	| SM83_OR op_a_r {
		sect_ConstByte(0xB0 | $2);
	}
;

sm83_pop:
	SM83_POP reg_tt {
		sect_ConstByte(0xC1 | ($2 << 4));
	}
;

sm83_push:
	SM83_PUSH reg_tt {
		sect_ConstByte(0xC5 | ($2 << 4));
	}
;

sm83_res:
	SM83_RES reloc_3bit COMMA reg_r {
		uint8_t mask = static_cast<uint8_t>(0x80 | $4);
		$2.makeCheckBitIndex(mask);
		sect_ConstByte(0xCB);
		if (!$2.isKnown()) {
			sect_RelByte($2, 0);
		} else {
			sect_ConstByte(mask | ($2.value() << 3));
		}
	}
;

sm83_ret:
	SM83_RET {
		sect_ConstByte(0xC9);
	}
	| SM83_RET ccode_expr {
		sect_ConstByte(0xC0 | ($2 << 3));
	}
;

sm83_reti:
	SM83_RETI {
		sect_ConstByte(0xD9);
	}
;

sm83_rl:
	SM83_RL reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x10 | $2);
	}
;

sm83_rla:
	SM83_RLA {
		sect_ConstByte(0x17);
	}
;

sm83_rlc:
	SM83_RLC reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x00 | $2);
	}
;

sm83_rlca:
	SM83_RLCA {
		sect_ConstByte(0x07);
	}
;

sm83_rr:
	SM83_RR reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x18 | $2);
	}
;

sm83_rra:
	SM83_RRA {
		sect_ConstByte(0x1F);
	}
;

sm83_rrc:
	SM83_RRC reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x08 | $2);
	}
;

sm83_rrca:
	SM83_RRCA {
		sect_ConstByte(0x0F);
	}
;

sm83_rst:
	SM83_RST reloc_8bit {
		$2.makeCheckRST();
		if (!$2.isKnown()) {
			sect_RelByte($2, 0);
		} else {
			sect_ConstByte(0xC7 | $2.value());
		}
	}
;

sm83_sbc:
	SM83_SBC op_a_n {
		sect_ConstByte(0xDE);
		sect_RelByte($2, 1);
	}
	| SM83_SBC op_a_r {
		sect_ConstByte(0x98 | $2);
	}
;

sm83_scf:
	SM83_SCF {
		sect_ConstByte(0x37);
	}
;

sm83_set:
	SM83_SET reloc_3bit COMMA reg_r {
		uint8_t mask = static_cast<uint8_t>(0xC0 | $4);
		$2.makeCheckBitIndex(mask);
		sect_ConstByte(0xCB);
		if (!$2.isKnown()) {
			sect_RelByte($2, 0);
		} else {
			sect_ConstByte(mask | ($2.value() << 3));
		}
	}
;

sm83_sla:
	SM83_SLA reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x20 | $2);
	}
;

sm83_sra:
	SM83_SRA reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x28 | $2);
	}
;

sm83_srl:
	SM83_SRL reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x38 | $2);
	}
;

sm83_stop:
	SM83_STOP {
		sect_ConstByte(0x10);
		sect_ConstByte(0x00);
	}
	| SM83_STOP reloc_8bit {
		sect_ConstByte(0x10);
		sect_RelByte($2, 1);
	}
;

sm83_sub:
	SM83_SUB op_a_n {
		sect_ConstByte(0xD6);
		sect_RelByte($2, 1);
	}
	| SM83_SUB op_a_r {
		sect_ConstByte(0x90 | $2);
	}
;

sm83_swap:
	SM83_SWAP reg_r {
		sect_ConstByte(0xCB);
		sect_ConstByte(0x30 | $2);
	}
;

sm83_xor:
	SM83_XOR op_a_n {
		sect_ConstByte(0xEE);
		sect_RelByte($2, 1);
	}
	| SM83_XOR op_a_r {
		sect_ConstByte(0xA8 | $2);
	}
;

// Registers or values.

op_mem_ind:
	LBRACK reloc_16bit RBRACK {
		$$ = std::move($2);
	}
;

op_a_r:
	  reg_r
	| MODE_A COMMA reg_r {
		$$ = $3;
	}
;

op_a_n:
	reloc_8bit {
		$$ = std::move($1);
	}
	| MODE_A COMMA reloc_8bit {
		$$ = std::move($3);
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

reg_r: reg_r_no_a | reg_a;

reg_r_no_a:
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
;

reg_a:
	MODE_A {
		$$ = REG_A;
	}
;

reg_tt:
	reg_tt_no_af
	| MODE_AF {
		$$ = REG_AF;
	}
;

reg_ss:
	reg_tt_no_af
	| MODE_SP {
		$$ = REG_SP;
	}
;

reg_tt_no_af:
	reg_bc_or_de
	| MODE_HL {
		$$ = REG_HL;
	}
;

reg_bc_or_de:
	MODE_BC {
		$$ = REG_BC;
	}
	| MODE_DE {
		$$ = REG_DE;
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

/******************** Semantic actions ********************/

void yy::parser::error(std::string const &str) {
	::error("%s", str.c_str());
}

static uint32_t strToNum(std::vector<int32_t> const &s) {
	uint32_t length = s.size();

	if (length == 1) {
		// The string is a single character with a single value,
		// which can be used directly as a number.
		return static_cast<uint32_t>(s[0]);
	}

	warning(WARNING_OBSOLETE, "Treating multi-unit strings as numbers is deprecated");

	for (int32_t v : s) {
		if (!checkNBit(v, 8, "All character units")) {
			break;
		}
	}

	uint32_t r = 0;

	for (uint32_t i = length < 4 ? 0 : length - 4; i < length; i++) {
		r <<= 8;
		r |= static_cast<uint8_t>(s[i]);
	}

	return r;
}

static void errorInvalidUTF8Byte(uint8_t byte, char const *functionName) {
	error("%s: Invalid UTF-8 byte 0x%02hhX", functionName, byte);
}

static size_t strlenUTF8(std::string const &str, bool printErrors) {
	char const *ptr = str.c_str();
	size_t len = 0;
	uint32_t state = 0;

	for (uint32_t codepoint = 0; *ptr; ptr++) {
		uint8_t byte = *ptr;

		switch (decode(&state, &codepoint, byte)) {
		case 1:
			if (printErrors) {
				errorInvalidUTF8Byte(byte, "STRLEN");
			}
			state = 0;
			// fallthrough
		case 0:
			len++;
			break;
		}
	}

	// Check for partial code point.
	if (state != 0) {
		if (printErrors) {
			error("STRLEN: Incomplete UTF-8 character");
		}
		len++;
	}

	return len;
}

static std::string strsliceUTF8(std::string const &str, uint32_t start, uint32_t stop) {
	char const *ptr = str.c_str();
	size_t index = 0;
	uint32_t state = 0;
	uint32_t codepoint = 0;
	uint32_t curIdx = 0;

	// Advance to starting index in source string.
	while (ptr[index] && curIdx < start) {
		switch (decode(&state, &codepoint, ptr[index])) {
		case 1:
			errorInvalidUTF8Byte(ptr[index], "STRSLICE");
			state = 0;
			// fallthrough
		case 0:
			curIdx++;
			break;
		}
		index++;
	}

	// An index 1 past the end of the string is allowed, but will trigger the
	// "Length too big" warning below if the length is nonzero.
	if (!ptr[index] && start > curIdx) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "STRSLICE: Start index %" PRIu32 " is past the end of the string",
		    start
		);
	}

	size_t startIndex = index;

	// Advance to ending index in source string.
	while (ptr[index] && curIdx < stop) {
		switch (decode(&state, &codepoint, ptr[index])) {
		case 1:
			errorInvalidUTF8Byte(ptr[index], "STRSLICE");
			state = 0;
			// fallthrough
		case 0:
			curIdx++;
			break;
		}
		index++;
	}

	// Check for partial code point.
	if (state != 0) {
		error("STRSLICE: Incomplete UTF-8 character");
		curIdx++;
	}

	if (curIdx < stop) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "STRSLICE: Stop index %" PRIu32 " is past the end of the string",
		    stop
		);
	}

	return std::string(ptr + startIndex, ptr + index);
}

static std::string strsubUTF8(std::string const &str, uint32_t pos, uint32_t len) {
	char const *ptr = str.c_str();
	size_t index = 0;
	uint32_t state = 0;
	uint32_t codepoint = 0;
	uint32_t curPos = 1;

	// Advance to starting position in source string.
	while (ptr[index] && curPos < pos) {
		switch (decode(&state, &codepoint, ptr[index])) {
		case 1:
			errorInvalidUTF8Byte(ptr[index], "STRSUB");
			state = 0;
			// fallthrough
		case 0:
			curPos++;
			break;
		}
		index++;
	}

	// A position 1 past the end of the string is allowed, but will trigger the
	// "Length too big" warning below if the length is nonzero.
	if (!ptr[index] && pos > curPos) {
		warning(
		    WARNING_BUILTIN_ARG, "STRSUB: Position %" PRIu32 " is past the end of the string", pos
		);
	}

	size_t startIndex = index;
	uint32_t curLen = 0;

	// Compute the result length in bytes.
	while (ptr[index] && curLen < len) {
		switch (decode(&state, &codepoint, ptr[index])) {
		case 1:
			errorInvalidUTF8Byte(ptr[index], "STRSUB");
			state = 0;
			// fallthrough
		case 0:
			curLen++;
			break;
		}
		index++;
	}

	// Check for partial code point.
	if (state != 0) {
		error("STRSUB: Incomplete UTF-8 character");
		curLen++;
	}

	if (curLen < len) {
		warning(WARNING_BUILTIN_ARG, "STRSUB: Length too big: %" PRIu32, len);
	}

	return std::string(ptr + startIndex, ptr + index);
}

static size_t charlenUTF8(std::string const &str) {
	std::string_view view = str;
	size_t len;

	for (len = 0; charmap_ConvertNext(view, nullptr); len++) {}

	return len;
}

static std::string strcharUTF8(std::string const &str, uint32_t idx) {
	std::string_view view = str;
	size_t charLen = 1;

	// Advance to starting index in source string.
	for (uint32_t curIdx = 0; charLen && curIdx < idx; curIdx++) {
		charLen = charmap_ConvertNext(view, nullptr);
	}

	std::string_view start = view;

	if (!charmap_ConvertNext(view, nullptr)) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "STRCHAR: Index %" PRIu32 " is past the end of the string",
		    idx
		);
	}

	start = start.substr(0, start.length() - view.length());
	return std::string(start);
}

static std::string charsubUTF8(std::string const &str, uint32_t pos) {
	std::string_view view = str;
	size_t charLen = 1;

	// Advance to starting position in source string.
	for (uint32_t curPos = 1; charLen && curPos < pos; curPos++) {
		charLen = charmap_ConvertNext(view, nullptr);
	}

	std::string_view start = view;

	if (!charmap_ConvertNext(view, nullptr)) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "CHARSUB: Position %" PRIu32 " is past the end of the string",
		    pos
		);
	}

	start = start.substr(0, start.length() - view.length());
	return std::string(start);
}

static int32_t charcmp(std::string_view str1, std::string_view str2) {
	std::vector<int32_t> seq1, seq2;
	size_t idx1 = 0, idx2 = 0;
	for (;;) {
		if (idx1 >= seq1.size()) {
			idx1 = 0;
			seq1.clear();
			charmap_ConvertNext(str1, &seq1);
		}
		if (idx2 >= seq2.size()) {
			idx2 = 0;
			seq2.clear();
			charmap_ConvertNext(str2, &seq2);
		}
		if (seq1.empty() != seq2.empty()) {
			return seq1.empty() ? -1 : 1;
		} else if (seq1.empty()) {
			return 0;
		} else {
			int32_t value1 = seq1[idx1++], value2 = seq2[idx2++];
			if (value1 != value2) {
				return (value1 > value2) - (value1 < value2);
			}
		}
	}
}

static uint32_t adjustNegativeIndex(int32_t idx, size_t len, char const *functionName) {
	// String functions adjust negative index arguments the same way,
	// such that position -1 is the last character of a string.
	if (idx < 0) {
		idx += len;
	}
	if (idx < 0) {
		warning(WARNING_BUILTIN_ARG, "%s: Index starts at 0", functionName);
		idx = 0;
	}
	return static_cast<uint32_t>(idx);
}

static uint32_t adjustNegativePos(int32_t pos, size_t len, char const *functionName) {
	// STRSUB and CHARSUB adjust negative position arguments the same way,
	// such that position -1 is the last character of a string.
	if (pos < 0) {
		pos += len + 1;
	}
	if (pos < 1) {
		warning(WARNING_BUILTIN_ARG, "%s: Position starts at 1", functionName);
		pos = 1;
	}
	return static_cast<uint32_t>(pos);
}

static std::string strrpl(std::string_view str, std::string const &old, std::string const &rep) {
	if (old.empty()) {
		warning(WARNING_EMPTY_STRRPL, "STRRPL: Cannot replace an empty string");
		return std::string(str);
	}

	std::string rpl;

	while (!str.empty()) {
		auto pos = str.find(old);
		if (pos == str.npos) {
			rpl.append(str);
			break;
		}
		rpl.append(str, 0, pos);
		rpl.append(rep);
		str.remove_prefix(pos + old.size());
	}

	return rpl;
}

static std::string
    strfmt(std::string const &spec, std::vector<std::variant<uint32_t, std::string>> const &args) {
	std::string str;
	size_t argIndex = 0;

	for (size_t i = 0; spec[i] != '\0'; ++i) {
		int c = spec[i];

		if (c != '%') {
			str += c;
			continue;
		}

		c = spec[++i];

		if (c == '%') {
			str += c;
			continue;
		}

		FormatSpec fmt{};

		while (c != '\0') {
			fmt.useCharacter(c);
			if (fmt.isFinished()) {
				break;
			}
			c = spec[++i];
		}

		if (fmt.isEmpty()) {
			error("STRFMT: Illegal '%%' at end of format string");
			str += '%';
			break;
		}

		if (!fmt.isValid()) {
			error("STRFMT: Invalid format spec for argument %zu", argIndex + 1);
			str += '%';
		} else if (argIndex >= args.size()) {
			// Will warn after formatting is done.
			str += '%';
		} else if (std::holds_alternative<uint32_t>(args[argIndex])) {
			fmt.appendNumber(str, std::get<uint32_t>(args[argIndex]));
		} else {
			fmt.appendString(str, std::get<std::string>(args[argIndex]));
		}

		argIndex++;
	}

	if (argIndex < args.size()) {
		error("STRFMT: %zu unformatted argument(s)", args.size() - argIndex);
	} else if (argIndex > args.size()) {
		error(
		    "STRFMT: Not enough arguments for format spec, got: %zu, need: %zu",
		    args.size(),
		    argIndex
		);
	}

	return str;
}

static void compoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue) {
	Expression oldExpr, constExpr, newExpr;
	int32_t newValue;

	oldExpr.makeSymbol(symName);
	constExpr.makeNumber(constValue);
	newExpr.makeBinaryOp(op, std::move(oldExpr), constExpr);
	newValue = newExpr.getConstVal();
	sym_AddVar(symName, newValue);
}

static void failAssert(AssertionType type) {
	switch (type) {
	case ASSERT_FATAL:
		fatalerror("Assertion failed");
	case ASSERT_ERROR:
		error("Assertion failed");
		break;
	case ASSERT_WARN:
		warning(WARNING_ASSERT, "Assertion failed");
		break;
	}
}

static void failAssertMsg(AssertionType type, std::string const &message) {
	switch (type) {
	case ASSERT_FATAL:
		fatalerror("Assertion failed: %s", message.c_str());
	case ASSERT_ERROR:
		error("Assertion failed: %s", message.c_str());
		break;
	case ASSERT_WARN:
		warning(WARNING_ASSERT, "Assertion failed: %s", message.c_str());
		break;
	}
}
