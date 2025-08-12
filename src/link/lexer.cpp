// SPDX-License-Identifier: MIT

#include "link/lexer.hpp"

#include <array>
#include <errno.h>
#include <fstream>
#include <inttypes.h>
#include <stdio.h>
#include <vector>

#include "helpers.hpp"
#include "itertools.hpp"
#include "style.hpp"
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

static void printStackEntry(LexerStackEntry const &context) {
	style_Set(stderr, STYLE_CYAN, true);
	fputs(context.path.c_str(), stderr);
	style_Set(stderr, STYLE_CYAN, false);
	fprintf(stderr, "(%" PRIu32 ")", context.lineNo);
}

void lexer_TraceCurrent() {
	size_t n = lexerStack.size();

	if (warnings.traceDepth == TRACE_COLLAPSE) {
		fputs("   ", stderr); // Just three spaces; the fourth will be handled by the loop
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, " %s ", i == 0 ? "at" : "<-");
			printStackEntry(lexerStack[n - i - 1]);
		}
		putc('\n', stderr);
	} else if (warnings.traceDepth == 0 || static_cast<size_t>(warnings.traceDepth) >= n) {
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printStackEntry(lexerStack[n - i - 1]);
			putc('\n', stderr);
		}
	} else {
		size_t last = warnings.traceDepth / 2;
		size_t first = warnings.traceDepth - last;
		size_t skipped = n - warnings.traceDepth;
		for (size_t i = 0; i < first; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printStackEntry(lexerStack[n - i - 1]);
			putc('\n', stderr);
		}
		style_Reset(stderr);
		fprintf(stderr, "    ...%zu more%s\n", skipped, last ? "..." : "");
		for (size_t i = n - last; i < n; ++i) {
			style_Reset(stderr);
			fputs("    <- ", stderr);
			printStackEntry(lexerStack[n - i - 1]);
			putc('\n', stderr);
		}
	}

	style_Reset(stderr);
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
		scriptError("No binary digits found after %s", prefix);
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
		scriptError("No octal digits found after %s", prefix);
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
		scriptError("No hexadecimal digits found after %s", prefix);
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
			return parseHexNumber("\"0x\"");
		case 'X':
			context.file.sbumpc();
			return parseHexNumber("\"0X\"");
		case 'o':
			context.file.sbumpc();
			return parseOctNumber("\"0o\"");
		case 'O':
			context.file.sbumpc();
			return parseOctNumber("\"0O\"");
		case 'b':
			context.file.sbumpc();
			return parseBinNumber("\"0b\"");
		case 'B':
			context.file.sbumpc();
			return parseBinNumber("\"0B");
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
			scriptError("Unterminated string");
			break;
		}
		context.file.sbumpc();
		if (c == '\\') {
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
	if (c == '"') {
		context.file.sbumpc();
	}
	return yy::parser::make_string(std::move(str));
}

yy::parser::symbol_type yylex() {
	LexerStackEntry &context = lexerStack.back();
	int c = context.file.sbumpc();

	// First, skip leading whitespace.
	while (isWhitespace(c)) {
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
	} else if (isDecDigit(c)) {
		return parseNumber(c);
	} else if (isIdentChar(c)) { // Note that we match these *after* digit characters!
		std::string ident = readIdent(c);

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
		if (auto search = sectTypes.find(ident); search != sectTypes.end()) {
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
		if (auto search = keywords.find(ident); search != keywords.end()) {
			return search->second();
		}

		scriptError("Unknown keyword `%s`", ident.c_str());
		return yylex();
	} else {
		scriptError("Unexpected character %s", printChar(c));
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

bool lexer_Init(char const *linkerScriptName) {
	if (LexerStackEntry &newContext = lexerStack.emplace_back(std::string(linkerScriptName));
	    !newContext.file.open(newContext.path, std::ios_base::in)) {
		error("Failed to open linker script \"%s\"", linkerScriptName);
		lexerStack.clear();
		return false;
	}
	return true;
}
