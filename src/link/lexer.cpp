// SPDX-License-Identifier: MIT

#include "link/lexer.hpp"

#include <errno.h>
#include <fstream>
#include <ios>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "backtrace.hpp"
#include "linkdefs.hpp"
#include "util.hpp"

#include "link/warning.hpp"
// Include this last so it gets all type & constant definitions
#include "script.hpp" // For token definitions, generated from script.y

struct LexerStackEntry {
	std::filebuf file;
	std::string path;
	uint32_t lineNo;

	explicit LexerStackEntry(std::string &&path_) : file(), path(path_), lineNo(1) {}
};

static std::vector<LexerStackEntry> lexerStack;

void lexer_TraceCurrent() {
	trace_PrintBacktrace(
	    lexerStack,
	    [](LexerStackEntry const &context) { return context.path.c_str(); },
	    [](LexerStackEntry const &context) { return context.lineNo; }
	);
}

void lexer_IncludeFile(std::string &&path) {
	// `.emplace_back` can invalidate references to the stack's elements!
	// This is why `newContext` must be gotten before `prevContext`.
	LexerStackEntry &newContext = lexerStack.emplace_back(std::move(path));
	LexerStackEntry &prevContext = lexerStack[lexerStack.size() - 2];

	if (!newContext.file.open(newContext.path, std::ios_base::in)) {
		// `.pop_back()` will invalidate `newContext`, which is why `path` must be moved first.
		std::string badPath = std::move(newContext.path);
		lexerStack.pop_back();
		// This error will occur in `prevContext`, *before* incrementing the line number!
		scriptError(
		    "Failed to open included linker script \"%s\": %s", badPath.c_str(), strerror(errno)
		);
	}

	// `.pop_back()` cannot invalidate an unpopped reference, so `prevContext`
	// is still valid even if `.open()` failed.
	++prevContext.lineNo;
}

void lexer_IncLineNo() {
	++lexerStack.back().lineNo;
}

yy::parser::symbol_type yylex(); // Forward declaration for `yywrap`

static yy::parser::symbol_type yywrap() {
	static bool atEof = false;
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

static std::string readKeyword(int initial) {
	LexerStackEntry &context = lexerStack.back();
	std::string keyword;
	keyword.push_back(initial);
	for (int c = context.file.sgetc(); isAlphanumeric(c); c = context.file.snextc()) {
		keyword.push_back(c);
	}
	return keyword;
}

template<int Base, typename IsDigitFnT, typename ParseSomeDigitT>
static yy::parser::symbol_type parseSomeNumber(
    char const *prefix,
    int initial,
    char const *name,
    IsDigitFnT isSomeDigit,
    ParseSomeDigitT parseSomeDigit
) {
	LexerStackEntry &context = lexerStack.back();
	uint32_t number;
	if (prefix) {
		assume(initial == 0 && name != nullptr);
		int c = context.file.sgetc();
		if (!isSomeDigit(c)) {
			scriptError("No %s digits found after %s", name, prefix);
			return yy::parser::make_number(0);
		}
		number = parseSomeDigit(c);
		context.file.sbumpc();
	} else {
		assume(name == nullptr);
		number = parseSomeDigit(initial);
	}
	for (int c = context.file.sgetc(); isSomeDigit(c) || c == '_'; c = context.file.snextc()) {
		if (c != '_') {
			number = number * Base + parseSomeDigit(c);
		}
	}
	return yy::parser::make_number(number);
}

static yy::parser::symbol_type parseDecNumber(int initial) {
	return parseSomeNumber<10>(nullptr, initial, nullptr, isDigit, parseDigit);
}

static yy::parser::symbol_type parseBinNumber(char const *prefix) {
	return parseSomeNumber<2>(prefix, 0, "binary", isBinDigit, parseDigit);
}

static yy::parser::symbol_type parseOctNumber(char const *prefix) {
	return parseSomeNumber<8>(prefix, 0, "octal", isOctDigit, parseDigit);
}

static yy::parser::symbol_type parseHexNumber(char const *prefix) {
	return parseSomeNumber<16>(prefix, 0, "hexadecimal", isHexDigit, parseHexDigit);
}

static yy::parser::symbol_type parseAnyNumber(int initial) {
	LexerStackEntry &context = lexerStack.back();
	if (initial == '0') {
		switch (context.file.sgetc()) {
		case 'x':
		case 'X':
			context.file.sbumpc();
			return parseHexNumber("\"0x\"");
		case 'o':
		case 'O':
			context.file.sbumpc();
			return parseOctNumber("\"0o\"");
		case 'b':
		case 'B':
			context.file.sbumpc();
			return parseBinNumber("\"0b\"");
		}
	}
	return parseDecNumber(initial);
}

static yy::parser::symbol_type parseString() {
	LexerStackEntry &context = lexerStack.back();
	std::string str;
	for (int c = context.file.sgetc();; c = context.file.sgetc()) {
		if (c == EOF || isNewline(c)) {
			scriptError("Unterminated string");
			break;
		}
		context.file.sbumpc();
		if (c == '"') {
			break;
		} else if (c == '\\') {
			c = context.file.sgetc();
			if (c == EOF || isNewline(c)) {
				scriptError("Unterminated string");
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
				scriptError("Cannot escape character %s", printChar(c));
			}
			context.file.sbumpc();
		}
		str.push_back(c);
	}
	return yy::parser::make_string(std::move(str));
}

yy::parser::symbol_type yylex() {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sbumpc();

	// First, skip leading blank space.
	while (isBlankSpace(c)) {
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
		return parseHexNumber("'$'");
	} else if (c == '%') {
		return parseBinNumber("'%'");
	} else if (c == '&') {
		return parseOctNumber("'&'");
	} else if (isDigit(c)) {
		return parseAnyNumber(c);
	} else if (isLetter(c)) {
		std::string keyword = readKeyword(c);

		static UpperMap<SectionType> const sectTypes{
		    {"WRAM0", SECTTYPE_WRAM0},
		    {"VRAM",  SECTTYPE_VRAM },
		    {"ROMX",  SECTTYPE_ROMX },
		    {"ROM0",  SECTTYPE_ROM0 },
		    {"HRAM",  SECTTYPE_HRAM },
		    {"WRAMX", SECTTYPE_WRAMX},
		    {"SRAM",  SECTTYPE_SRAM },
		    {"OAM",   SECTTYPE_OAM  },
		};
		if (auto search = sectTypes.find(keyword); search != sectTypes.end()) {
			return yy::parser::make_sect_type(search->second);
		}

		static UpperMap<yy::parser::symbol_type (*)()> const keywords{
		    {"ORG",      yy::parser::make_ORG     },
		    {"FLOATING", yy::parser::make_FLOATING},
		    {"INCLUDE",  yy::parser::make_INCLUDE },
		    {"ALIGN",    yy::parser::make_ALIGN   },
		    {"DS",       yy::parser::make_DS      },
		    {"OPTIONAL", yy::parser::make_OPTIONAL},
		};
		if (auto search = keywords.find(keyword); search != keywords.end()) {
			return search->second();
		}

		scriptError("Unknown keyword `%s`", keyword.c_str());
		return yylex();
	} else {
		scriptError("Unexpected character %s", printChar(c));
		// Keep reading characters until the EOL, to avoid reporting too many errors.
		for (c = context.file.sgetc(); c != EOF && !isNewline(c); c = context.file.snextc()) {}
		return yylex();
	}
	// Not marking as unreachable; this will generate a warning if any codepath forgets to return.
}

bool lexer_Init(std::string const &linkerScriptName) {
	if (LexerStackEntry &newContext = lexerStack.emplace_back(std::string(linkerScriptName));
	    !newContext.file.open(newContext.path, std::ios_base::in)) {
		error("Failed to open linker script \"%s\"", linkerScriptName.c_str());
		lexerStack.clear();
		return false;
	}
	return true;
}
