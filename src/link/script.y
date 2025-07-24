// SPDX-License-Identifier: MIT

%language "c++"
%define api.value.type variant
%define api.token.constructor

%code requires {
	#include <stdint.h>
	#include <string>

	#include "linkdefs.hpp"

	void script_ProcessScript();
}

%code {
	#include <algorithm>
	#include <array>
	#include <bit>
	#include <ctype.h>
	#include <fstream>
	#include <inttypes.h>
	#include <stdio.h>
	#include <string_view>
	#include <vector>

	#include "helpers.hpp"
	#include "itertools.hpp"
	#include "util.hpp"

	#include "link/main.hpp"
	#include "link/section.hpp"
	#include "link/warning.hpp"

	using namespace std::literals;

	static void includeFile(std::string &&path);
	static void incLineNo();

	static void setFloatingSectionType(SectionType type);
	static void setSectionType(SectionType type);
	static void setSectionType(SectionType type, uint32_t bank);
	static void setAddr(uint32_t addr);
	static void makeAddrFloating();
	static void alignTo(uint32_t alignment, uint32_t offset);
	static void pad(uint32_t length);
	static void placeSection(std::string const &name, bool isOptional);

	static yy::parser::symbol_type yylex();

	struct Keyword {
		std::string_view name;
		yy::parser::symbol_type (*tokenGen)();
	};
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

%code {
	static std::array keywords{
		Keyword{"ORG"sv,      yy::parser::make_ORG},
		Keyword{"FLOATING"sv, yy::parser::make_FLOATING},
		Keyword{"INCLUDE"sv,  yy::parser::make_INCLUDE},
		Keyword{"ALIGN"sv,    yy::parser::make_ALIGN},
		Keyword{"DS"sv,       yy::parser::make_DS},
		Keyword{"OPTIONAL"sv, yy::parser::make_OPTIONAL},
	};
}

%%

/******************** Parser rules ********************/

lines:
	  %empty
	| line lines
;

line:
	INCLUDE string newline {
		includeFile(std::move($2)); // Note: this additionally increments the line number!
	}
	| directive newline {
		incLineNo();
	}
	| newline {
		incLineNo();
	}
	// Error recovery.
	| error newline {
		yyerrok;
		incLineNo();
	}
;

directive:
	sect_type {
		setSectionType($1);
	}
	| sect_type number {
		setSectionType($1, $2);
	}
	| sect_type FLOATING {
		setFloatingSectionType($1);
	}
	| FLOATING {
		makeAddrFloating();
	}
	| ORG number {
		setAddr($2);
	}
	| ALIGN number {
		alignTo($2, 0);
	}
	| ALIGN number COMMA number {
		alignTo($2, $4);
	}
	| DS number {
		pad($2);
	}
	| string optional {
		placeSection($1, $2);
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

#define scriptError(context, fmt, ...) \
	::error( \
	    "%s(%" PRIu32 "): " fmt, \
	    context.path.c_str(), \
	    context.lineNo __VA_OPT__(, ) __VA_ARGS__ \
	)

/******************** Lexer ********************/

struct LexerStackEntry {
	std::filebuf file;
	std::string path;
	uint32_t lineNo;

	explicit LexerStackEntry(std::string &&path_) : file(), path(path_), lineNo(1) {}
};
static std::vector<LexerStackEntry> lexerStack;
static bool atEof;

void yy::parser::error(std::string const &msg) {
	LexerStackEntry const &script = lexerStack.back();
	scriptError(script, "%s", msg.c_str());
}

static void includeFile(std::string &&path) {
	// `emplace_back` can invalidate references to the stack's elements!
	// This is why `newContext` must be gotten before `prevContext`.
	LexerStackEntry &newContext = lexerStack.emplace_back(std::move(path));
	LexerStackEntry &prevContext = lexerStack[lexerStack.size() - 2];

	if (!newContext.file.open(newContext.path, std::ios_base::in)) {
		// The order is important: report the error, increment the line number, modify the stack!
		scriptError(
		    prevContext, "Failed to open included linker script \"%s\"", newContext.path.c_str()
		);
		++prevContext.lineNo;
		lexerStack.pop_back();
	} else {
		// The lexer will use the new entry to lex the next token.
		++prevContext.lineNo;
	}
}

static void incLineNo() {
	++lexerStack.back().lineNo;
}

static bool isWhiteSpace(int c) {
	return c == ' ' || c == '\t';
}

static bool isNewline(int c) {
	return c == '\r' || c == '\n';
}

static yy::parser::symbol_type yywrap() {
	if (lexerStack.size() != 1) {
		if (!atEof) {
			// Inject a newline at EOF to simplify parsing.
			atEof = true;
			return yy::parser::make_newline();
		}
		lexerStack.pop_back();
		return yylex();
	}
	if (!atEof) {
		// Inject a newline at EOF to simplify parsing.
		atEof = true;
		return yy::parser::make_newline();
	}
	return yy::parser::make_YYEOF();
}

static bool isIdentChar(int c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static std::string readIdent(int c) {
	LexerStackEntry &context = lexerStack.back();
	std::string ident;
	ident.push_back(c);
	for (c = context.file.sgetc(); isIdentChar(c); c = context.file.snextc()) {
		ident.push_back(c);
	}
	return ident;
}

static bool isDecDigit(int c) {
	return c >= '0' && c <= '9';
}

static yy::parser::symbol_type parseDecNumber(int c) {
	LexerStackEntry &context = lexerStack.back();
	uint32_t number = c - '0';
	for (c = context.file.sgetc(); isDecDigit(c) || c == '_'; c = context.file.sgetc()) {
		if (c != '_') {
			number = number * 10 + (c - '0');
		}
		context.file.sbumpc();
	}
	return yy::parser::make_number(number);
}

static bool isBinDigit(int c) {
	return c >= '0' && c <= '1';
}

static yy::parser::symbol_type parseBinNumber(char const *prefix) {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sgetc();
	if (!isBinDigit(c)) {
		scriptError(context, "No binary digits found after '%s'", prefix);
		return yy::parser::make_number(0);
	}

	uint32_t number = c - '0';
	context.file.sbumpc();
	for (c = context.file.sgetc(); isBinDigit(c) || c == '_'; c = context.file.sgetc()) {
		if (c != '_') {
			number = number * 2 + (c - '0');
		}
		context.file.sbumpc();
	}
	return yy::parser::make_number(number);
}

static bool isOctDigit(int c) {
	return c >= '0' && c <= '7';
}

static yy::parser::symbol_type parseOctNumber(char const *prefix) {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sgetc();
	if (!isOctDigit(c)) {
		scriptError(context, "No octal digits found after '%s'", prefix);
		return yy::parser::make_number(0);
	}

	uint32_t number = c - '0';
	context.file.sbumpc();
	for (c = context.file.sgetc(); isOctDigit(c) || c == '_'; c = context.file.sgetc()) {
		if (c != '_') {
			number = number * 8 + (c - '0');
		}
		context.file.sbumpc();
	}
	return yy::parser::make_number(number);
}

static bool isHexDigit(int c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static uint8_t parseHexDigit(int c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else {
		unreachable_(); // LCOV_EXCL_LINE
	}
}

static yy::parser::symbol_type parseHexNumber(char const *prefix) {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sgetc();
	if (!isHexDigit(c)) {
		scriptError(context, "No hexadecimal digits found after '%s'", prefix);
		return yy::parser::make_number(0);
	}

	uint32_t number = parseHexDigit(c);
	context.file.sbumpc();
	for (c = context.file.sgetc(); isHexDigit(c) || c == '_'; c = context.file.sgetc()) {
		if (c != '_') {
			number = number * 16 + parseHexDigit(c);
		}
		context.file.sbumpc();
	}
	return yy::parser::make_number(number);
}

static yy::parser::symbol_type parseNumber(int c) {
	LexerStackEntry &context = lexerStack.back();
	if (c == '0') {
		switch (context.file.sgetc()) {
		case 'x':
			context.file.sbumpc();
			return parseHexNumber("0x");
		case 'X':
			context.file.sbumpc();
			return parseHexNumber("0X");
		case 'o':
			context.file.sbumpc();
			return parseOctNumber("0o");
		case 'O':
			context.file.sbumpc();
			return parseOctNumber("0O");
		case 'b':
			context.file.sbumpc();
			return parseBinNumber("0b");
		case 'B':
			context.file.sbumpc();
			return parseBinNumber("0B");
		}
	}
	return parseDecNumber(c);
}

static yy::parser::symbol_type parseString() {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sgetc();
	std::string str;
	for (; c != '"'; c = context.file.sgetc()) {
		if (c == EOF || isNewline(c)) {
			scriptError(context, "Unterminated string");
			break;
		}
		context.file.sbumpc();
		if (c == '\\') {
			c = context.file.sgetc();
			if (c == EOF || isNewline(c)) {
				scriptError(context, "Unterminated string");
				break;
			} else if (c == 'n') {
				c = '\n';
			} else if (c == 'r') {
				c = '\r';
			} else if (c == 't') {
				c = '\t';
			} else if (c == '0') {
				c = '\0';
			} else if (c != '\\' && c != '"' && c != '\'') {
				scriptError(context, "Cannot escape character %s", printChar(c));
			}
			context.file.sbumpc();
		}
		str.push_back(c);
	}
	if (c == '"') {
		context.file.sbumpc();
	}
	return yy::parser::make_string(std::move(str));
}

yy::parser::symbol_type yylex() {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sbumpc();

	// First, skip leading whitespace.
	while (isWhiteSpace(c)) {
		c = context.file.sbumpc();
	}
	// Then, skip a comment if applicable.
	if (c == ';') {
		while (c != EOF && !isNewline(c)) {
			c = context.file.sbumpc();
		}
	}

	// Alright, what token should we return?
	if (c == EOF) {
		return yywrap();
	} else if (c == ',') {
		return yy::parser::make_COMMA();
	} else if (isNewline(c)) {
		// Handle CRLF.
		if (c == '\r' && context.file.sgetc() == '\n') {
			context.file.sbumpc();
		}
		return yy::parser::make_newline();
	} else if (c == '"') {
		return parseString();
	} else if (c == '$') {
		return parseHexNumber("$");
	} else if (c == '%') {
		return parseBinNumber("%");
	} else if (c == '&') {
		return parseOctNumber("&");
	} else if (isDecDigit(c)) {
		return parseNumber(c);
	} else if (isIdentChar(c)) { // Note that we match these *after* digit characters!
		std::string ident = readIdent(c);

		auto strUpperCmp = [](char cmp, char ref) {
			return toupper(cmp) == ref;
		};

		for (SectionType type : EnumSeq(SECTTYPE_INVALID)) {
			if (std::equal(RANGE(ident), RANGE(sectionTypeInfo[type].name), strUpperCmp)) {
				return yy::parser::make_sect_type(type);
			}
		}

		for (Keyword const &keyword : keywords) {
			if (std::equal(RANGE(ident), RANGE(keyword.name), strUpperCmp)) {
				return keyword.tokenGen();
			}
		}

		scriptError(context, "Unknown keyword \"%s\"", ident.c_str());
		return yylex();
	} else {
		scriptError(context, "Unexpected character %s", printChar(c));
		// Keep reading characters until the EOL, to avoid reporting too many errors.
		for (c = context.file.sgetc(); !isNewline(c); c = context.file.sgetc()) {
			if (c == EOF) {
				break;
			}
			context.file.sbumpc();
		}
		return yylex();
	}
	// Not marking as unreachable; this will generate a warning if any codepath forgets to return.
}

/******************** Semantic actions ********************/

static std::array<std::vector<uint16_t>, SECTTYPE_INVALID> curAddr;
static SectionType activeType; // Index into curAddr
static uint32_t activeBankIdx; // Index into curAddr[activeType]
static bool isPcFloating;
static uint16_t floatingAlignMask;
static uint16_t floatingAlignOffset;

static void setActiveTypeAndIdx(SectionType type, uint32_t idx) {
	activeType = type;
	activeBankIdx = idx;
	isPcFloating = false;
	if (curAddr[activeType].size() <= activeBankIdx) {
		curAddr[activeType].resize(activeBankIdx + 1, sectionTypeInfo[type].startAddr);
	}
}

static void setFloatingSectionType(SectionType type) {
	if (nbbanks(type) == 1) {
		// There is only a single bank anyway, so just set the index to 0.
		setActiveTypeAndIdx(type, 0);
	} else {
		activeType = type;
		activeBankIdx = UINT32_MAX;
		// Force PC to be floating for this kind of section.
		// Because we wouldn't know how to index into `curAddr[activeType]`!
		isPcFloating = true;
		floatingAlignMask = 0;
		floatingAlignOffset = 0;
	}
}

static void setSectionType(SectionType type) {
	LexerStackEntry const &context = lexerStack.back();

	if (nbbanks(type) != 1) {
		scriptError(
		    context, "A bank number must be specified for %s", sectionTypeInfo[type].name.c_str()
		);
		// Keep going with a default value for the bank index.
	}

	setActiveTypeAndIdx(type, 0); // There is only a single bank anyway, so just set the index to 0.
}

static void setSectionType(SectionType type, uint32_t bank) {
	LexerStackEntry const &context = lexerStack.back();
	SectionTypeInfo const &typeInfo = sectionTypeInfo[type];

	if (bank < typeInfo.firstBank) {
		scriptError(
		    context,
		    "%s bank %" PRIu32 " doesn't exist (the minimum is %" PRIu32 ")",
		    typeInfo.name.c_str(),
		    bank,
		    typeInfo.firstBank
		);
		bank = typeInfo.firstBank;
	} else if (bank > typeInfo.lastBank) {
		scriptError(
		    context,
		    "%s bank %" PRIu32 " doesn't exist (the maximum is %" PRIu32 ")",
		    typeInfo.name.c_str(),
		    bank,
		    typeInfo.lastBank
		);
	}

	setActiveTypeAndIdx(type, bank - typeInfo.firstBank);
}

static void setAddr(uint32_t addr) {
	LexerStackEntry const &context = lexerStack.back();
	if (activeType == SECTTYPE_INVALID) {
		scriptError(context, "Cannot set the current address: no memory region is active");
		return;
	}
	if (activeBankIdx == UINT32_MAX) {
		scriptError(context, "Cannot set the current address: the bank is floating");
		return;
	}

	uint16_t &pc = curAddr[activeType][activeBankIdx];
	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];

	if (addr < pc) {
		scriptError(context, "Cannot decrease the current address (from $%04x to $%04x)", pc, addr);
	} else if (addr > endaddr(activeType)) { // Allow "one past the end" sections.
		scriptError(
		    context,
		    "Cannot set the current address to $%04" PRIx32 ": %s ends at $%04" PRIx16 "",
		    addr,
		    typeInfo.name.c_str(),
		    endaddr(activeType)
		);
		pc = endaddr(activeType);
	} else {
		pc = addr;
	}
	isPcFloating = false;
}

static void makeAddrFloating() {
	LexerStackEntry const &context = lexerStack.back();
	if (activeType == SECTTYPE_INVALID) {
		scriptError(
		    context, "Cannot make the current address floating: no memory region is active"
		);
		return;
	}

	isPcFloating = true;
	floatingAlignMask = 0;
	floatingAlignOffset = 0;
}

static void alignTo(uint32_t alignment, uint32_t alignOfs) {
	LexerStackEntry const &context = lexerStack.back();
	if (activeType == SECTTYPE_INVALID) {
		scriptError(context, "Cannot align: no memory region is active");
		return;
	}

	if (isPcFloating) {
		if (alignment >= 16) {
			setAddr(floatingAlignOffset);
		} else {
			uint32_t alignSize = 1u << alignment;

			if (alignOfs >= alignSize) {
				scriptError(
				    context,
				    "Cannot align: The alignment offset (%" PRIu32
				    ") must be less than alignment size (%" PRIu32 ")",
				    alignOfs,
				    alignSize
				);
				return;
			}

			floatingAlignMask = alignSize - 1;
			floatingAlignOffset = alignOfs % alignSize;
		}
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	uint16_t &pc = curAddr[activeType][activeBankIdx];

	if (alignment > 16) {
		scriptError(
		    context, "Cannot align: The alignment (%" PRIu32 ") must be less than 16", alignment
		);
		return;
	}

	// Let it wrap around, this'll trip the final check if alignment == 16.
	uint16_t length = alignOfs - pc;

	if (alignment < 16) {
		uint32_t alignSize = 1u << alignment;

		if (alignOfs >= alignSize) {
			scriptError(
			    context,
			    "Cannot align: The alignment offset (%" PRIu32
			    ") must be less than alignment size (%" PRIu32 ")",
			    alignOfs,
			    alignSize
			);
			return;
		}

		assume(pc >= typeInfo.startAddr);
		length %= alignSize;
	}

	if (uint16_t offset = pc - typeInfo.startAddr; length > typeInfo.size - offset) {
		scriptError(
		    context,
		    "Cannot align: the next suitable address after $%04" PRIx16 " is $%04" PRIx16
		    ", past $%04" PRIx16,
		    pc,
		    static_cast<uint16_t>(pc + length),
		    static_cast<uint16_t>(endaddr(activeType) + 1)
		);
		return;
	}

	pc += length;
}

static void pad(uint32_t length) {
	LexerStackEntry const &context = lexerStack.back();
	if (activeType == SECTTYPE_INVALID) {
		scriptError(context, "Cannot increase the current address: no memory region is active");
		return;
	}

	if (isPcFloating) {
		floatingAlignOffset = (floatingAlignOffset + length) & floatingAlignMask;
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	uint16_t &pc = curAddr[activeType][activeBankIdx];

	assume(pc >= typeInfo.startAddr);
	if (uint16_t offset = pc - typeInfo.startAddr; length + offset > typeInfo.size) {
		scriptError(
		    context,
		    "Cannot increase the current address by %u bytes: only %u bytes to $%04" PRIx16,
		    length,
		    typeInfo.size - offset,
		    static_cast<uint16_t>(endaddr(activeType) + 1)
		);
	} else {
		pc += length;
	}
}

static void placeSection(std::string const &name, bool isOptional) {
	LexerStackEntry const &context = lexerStack.back();
	if (activeType == SECTTYPE_INVALID) {
		scriptError(
		    context, "No memory region has been specified to place section \"%s\" in", name.c_str()
		);
		return;
	}

	Section *section = sect_GetSection(name.c_str());
	if (!section) {
		if (!isOptional) {
			scriptError(context, "Unknown section \"%s\"", name.c_str());
		}
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	assume(section->offset == 0);
	// Check that the linker script doesn't contradict what the code says.
	if (section->type == SECTTYPE_INVALID) {
		// A section that has data must get assigned a type that requires data.
		if (!sect_HasData(activeType) && !section->data.empty()) {
			scriptError(
			    context,
			    "\"%s\" is specified to be a %s section, but it contains data",
			    name.c_str(),
			    typeInfo.name.c_str()
			);
		} else if (sect_HasData(activeType) && section->data.empty() && section->size != 0) {
			// A section that lacks data can only be assigned to a type that requires data
			// if it's empty.
			scriptError(
			    context,
			    "\"%s\" is specified to be a %s section, but it doesn't contain data",
			    name.c_str(),
			    typeInfo.name.c_str()
			);
		} else {
			// SDCC areas don't have a type assigned yet, so the linker script is used to give them
			// one.
			for (Section *fragment = section; fragment; fragment = fragment->nextu.get()) {
				fragment->type = activeType;
			}
		}
	} else if (section->type != activeType) {
		scriptError(
		    context,
		    "\"%s\" is specified to be a %s section, but it is already a %s section",
		    name.c_str(),
		    typeInfo.name.c_str(),
		    sectionTypeInfo[section->type].name.c_str()
		);
	}

	if (activeBankIdx == UINT32_MAX) {
		section->isBankFixed = false;
	} else {
		uint32_t bank = activeBankIdx + typeInfo.firstBank;
		if (section->isBankFixed && bank != section->bank) {
			scriptError(
			    context,
			    "The linker script places section \"%s\" in %s bank %" PRIu32
			    ", but it was already defined in bank %" PRIu32,
			    name.c_str(),
			    sectionTypeInfo[section->type].name.c_str(),
			    bank,
			    section->bank
			);
		}
		section->isBankFixed = true;
		section->bank = bank;
	}

	if (!isPcFloating) {
		uint16_t &org = curAddr[activeType][activeBankIdx];
		if (section->isAddressFixed && org != section->org) {
			scriptError(
			    context,
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but it was already at $%04" PRIx16,
			    name.c_str(),
			    org,
			    section->org
			);
		} else if (section->isAlignFixed && (org & section->alignMask) != section->alignOfs) {
			uint8_t alignment = std::countr_one(section->alignMask);
			scriptError(
			    context,
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but that would be ALIGN[%" PRIu8 ", %" PRIu16
			    "] instead of the requested ALIGN[%" PRIu8 ", %" PRIu16 "]",
			    name.c_str(),
			    org,
			    alignment,
			    static_cast<uint16_t>(org & section->alignMask),
			    alignment,
			    section->alignOfs
			);
		}
		section->isAddressFixed = true;
		section->isAlignFixed = false; // This can't be set when the above is.
		section->org = org;

		uint16_t curOfs = org - typeInfo.startAddr;
		if (section->size > typeInfo.size - curOfs) {
			uint16_t overflowSize = section->size - (typeInfo.size - curOfs);
			scriptError(
			    context,
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but then it would overflow %s by %" PRIu16 " byte%s",
			    name.c_str(),
			    org,
			    typeInfo.name.c_str(),
			    overflowSize,
			    overflowSize == 1 ? "" : "s"
			);
			// Fill as much as possible without going out of bounds.
			org = typeInfo.startAddr + typeInfo.size;
		} else {
			org += section->size;
		}
	} else {
		section->isAddressFixed = false;
		section->isAlignFixed = floatingAlignMask != 0;
		section->alignMask = floatingAlignMask;
		section->alignOfs = floatingAlignOffset;

		floatingAlignOffset = (floatingAlignOffset + section->size) & floatingAlignMask;
	}
}

/******************** External API ********************/

void script_ProcessScript() {
	activeType = SECTTYPE_INVALID;

	lexerStack.clear();
	atEof = false;
	LexerStackEntry &newContext = lexerStack.emplace_back(std::string(options.linkerScriptName));

	if (!newContext.file.open(newContext.path, std::ios_base::in)) {
		error("Failed to open linker script \"%s\"", newContext.path.c_str());
		lexerStack.clear();
	} else {
		yy::parser linkerScriptParser;
		// We don't care about the return value, as any error increments the global error count,
		// which is what `main` checks.
		(void)linkerScriptParser.parse();

		// Free up working memory.
		for (std::vector<uint16_t> &region : curAddr) {
			region.clear();
		}
	}
}
