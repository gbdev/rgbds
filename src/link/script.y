%language "c++"
%define api.value.type variant
%define api.token.constructor

%code requires {
	#include <stdint.h>
	#include <string>

	#include "linkdefs.hpp"
}
%code {
	#include <algorithm>
	#include <array>
	#include <assert.h>
	#include <bit>
	#include <cinttypes>
	#include <fstream>
	#include <locale>
	#include <string_view>
	#include <vector>

	#include "helpers.hpp"
	#include "itertools.hpp"
	#include "util.hpp"

	#include "link/main.hpp"
	#include "link/section.hpp"

	using namespace std::literals;

	static void includeFile(std::string &&path);
	static void incLineNo(void);

	static void setSectionType(SectionType type);
	static void setSectionType(SectionType type, uint32_t bank);
	static void setAddr(uint32_t addr);
	static void alignTo(uint32_t alignment, uint32_t offset);
	static void pad(uint32_t length);
	static void placeSection(std::string const &name);

	static yy::parser::symbol_type yylex(void);

	struct Keyword {
		std::string_view name;
		yy::parser::symbol_type (* tokenGen)(void);
	};
}

%token YYEOF 0
%token newline
%token ORG "ORG"
       INCLUDE "INCLUDE"
       ALIGN "ALIGN"
       DS "DS"
%code {
	static std::array keywords{
		Keyword{"ORG"sv,     yy::parser::make_ORG},
		Keyword{"INCLUDE"sv, yy::parser::make_INCLUDE},
		Keyword{"ALIGN"sv,   yy::parser::make_ALIGN},
		Keyword{"DS"sv,      yy::parser::make_DS},
	};
}
%token <std::string> string;
%token <uint32_t> number;
%token <SectionType> section_type;

%%

lines: %empty
     | line lines
;

line: INCLUDE string newline { includeFile(std::move($2)); } // Note: this additionally increments the line number!
    | directive newline { incLineNo(); }
    | newline { incLineNo(); }
    | error newline { yyerrok; incLineNo(); } // Error recovery.
;

directive: section_type { setSectionType($1); }
         | section_type number { setSectionType($1, $2); }
         | ORG number { setAddr($2); }
         | ALIGN number { alignTo($2, 0); }
         | DS number { pad($2); }
         | string { placeSection($1); }
;

%%

#define scriptError(context, fmt, ...) ::error(NULL, 0, "%s(%" PRIu32 "): " fmt, \
                                               context.path.c_str(), context.lineNo, __VA_ARGS__)
// MSVC doesn't support __VA_OPT__ yet.
#define scriptErrorSimple(context, str) ::error(NULL, 0, "%s(%" PRIu32 "): " str, \
                                               context.path.c_str(), context.lineNo)

// Lexer.

struct LexerStackEntry {
	std::filebuf file;
	std::string path;
	uint32_t lineNo;

	using int_type = decltype(file)::int_type;
	static constexpr int_type eof = decltype(file)::traits_type::eof();

	explicit LexerStackEntry(std::string &&path_) : file(), path(path_), lineNo(1) {}
};
static std::vector<LexerStackEntry> lexerStack;

void yy::parser::error(std::string const &msg) {
	auto const &script = lexerStack.back();
	scriptError(script, "%s", msg.c_str());
}

static void includeFile(std::string &&path) {
	auto &newContext = lexerStack.emplace_back(std::move(path));
	auto &prevContext = lexerStack[lexerStack.size() - 2];

	if (!newContext.file.open(newContext.path, std::ios_base::in)) {
		// The order is important: report the error, increment the line number, modify the stack!
		scriptError(prevContext, "Could not open included linker script \"%s\"",
		            newContext.path.c_str());
		++prevContext.lineNo;
		lexerStack.pop_back();
	} else {
		// The lexer will use the new entry to lex the next token.
		++prevContext.lineNo;
	}
}

static void incLineNo(void) {
	++lexerStack.back().lineNo;
}

static bool isWhiteSpace(LexerStackEntry::int_type c) {
	return c == ' ' || c == '\t';
}

static bool isNewline(LexerStackEntry::int_type c) {
	return c == '\r' || c == '\n';
}

static bool isIdentChar(LexerStackEntry::int_type c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static bool isHexDigit(LexerStackEntry::int_type c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static uint8_t parseHexDigit(LexerStackEntry::int_type c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else {
		unreachable_();
	}
}

yy::parser::symbol_type yylex(void) {
try_again: // Can't use a `do {} while(0)` loop, otherwise compilers (wrongly) think it can end.
	auto &context = lexerStack.back();
	auto c = context.file.sbumpc();

	// First, skip leading whitespace.
	while (isWhiteSpace(c)) {
		c = context.file.sbumpc();
	}
	// Then, skip a comment if applicable.
	if (c == ';') {
		while (!isNewline(c)) {
			c = context.file.sbumpc();
		}
	}

	// Alright, what token should we return?
	if (c == LexerStackEntry::eof) {
		// Basically yywrap().
		lexerStack.pop_back();
		if (!lexerStack.empty()) {
			goto try_again;
		}
		return yy::parser::make_YYEOF();
	} else if (isNewline(c)) {
		// Handle CRLF.
		if (c == '\r' && context.file.sgetc() == '\n') {
			context.file.sbumpc();
		}
		return yy::parser::make_newline();
	} else if (c == '"') {
		std::string str;

		for (c = context.file.sgetc(); c != '"'; c = context.file.sgetc()) {
			if (c == LexerStackEntry::eof || isNewline(c)) {
				scriptErrorSimple(context, "Unterminated string");
				break;
			}
			context.file.sbumpc();
			if (c == '\\') {
				c = context.file.sgetc();
				if (c == LexerStackEntry::eof || isNewline(c)) {
					scriptErrorSimple(context, "Unterminated string");
					break;
				} else if (c == 'n') {
					c = '\n';
				} else if (c == 'r') {
					c = '\r';
				} else if (c == 't') {
					c = '\t';
				} else if (c != '\\' && c != '"') {
					scriptError(context, "Cannot escape character %s", printChar(c));
				}
				context.file.sbumpc();
			}
			str.push_back(c);
		}
		context.file.sbumpc(); // Consume the closing quote.

		return yy::parser::make_string(std::move(str));
	} else if (c == '$') {
		c = context.file.sgetc();
		if (!isHexDigit(c)) {
			scriptErrorSimple(context, "No hexadecimal digits found after '$'");
			return yy::parser::make_number(0);
		}

		uint32_t number = parseHexDigit(c);
		context.file.sbumpc();
		for (c = context.file.sgetc(); isHexDigit(c); c = context.file.sgetc()) {
			number = number * 16 + parseHexDigit(c);
			context.file.sbumpc();
		}
		return yy::parser::make_number(number);
	} else if (c == '%') {
		c = context.file.sgetc();
		if (!(c >= '0' && c <= '1')) {
			scriptErrorSimple(context, "No binary digits found after '%%'");
			return yy::parser::make_number(0);
		}

		uint32_t number = c - '0';
		context.file.sbumpc();
		for (c = context.file.sgetc(); c >= '0' && c <= '1'; c = context.file.sgetc()) {
			number = number * 2 + (c - '0');
			context.file.sbumpc();
		}
		return yy::parser::make_number(number);
	} else if (c >= '0' && c <= '9') {
		uint32_t number = c - '0';
		for (c = context.file.sgetc(); c >= '0' && c <= '9'; c = context.file.sgetc()) {
			number = number * 10 + (c - '0');
		}
		return yy::parser::make_number(number);
	} else if (isIdentChar(c)) { // Note that we match these *after* digit characters!
		std::string ident;
		auto strCaseCmp = [](char cmp, char ref) {
			// `locale::classic()` yields the "C" locale.
			assert(!std::use_facet<std::ctype<char>>(std::locale::classic())
			       .is(std::ctype_base::lower, ref));
			return std::use_facet<std::ctype<char>>(std::locale::classic())
			       .toupper(cmp) == ref;
		};

		ident.push_back(c);
		for (c = context.file.sgetc(); isIdentChar(c); c = context.file.snextc()) {
			ident.push_back(c);
		}

		for (SectionType type : EnumSeq(SECTTYPE_INVALID)) {
			if (std::equal(ident.begin(), ident.end(),
			               sectionTypeInfo[type].name.begin(), sectionTypeInfo[type].name.end(),
			               strCaseCmp)) {
				return yy::parser::make_section_type(type);
			}
		}

		for (Keyword const &keyword : keywords) {
			if (std::equal(ident.begin(), ident.end(),
			               keyword.name.begin(), keyword.name.end(),
			               strCaseCmp)) {
				return keyword.tokenGen();
			}
		}

		scriptError(context, "Unknown keyword \"%s\"", ident.c_str());
		goto try_again; // Try lexing another token.
	} else {
		scriptError(context, "Unexpected character '%s'", printChar(c));
		// Keep reading characters until the EOL, to avoid reporting too many errors.
		for (c = context.file.sgetc(); !isNewline(c); c = context.file.sgetc()) {
			if (c == LexerStackEntry::eof) {
				break;
			}
		}
		goto try_again;
	}
	// Not marking as unreachable; this will generate a warning if any codepath forgets to return.
}

// Semantic actions.

static SectionType activeType;
static uint32_t activeBankIdx; // This is the index into the array, not
static std::array<std::vector<uint16_t>, SECTTYPE_INVALID> curAddr;

static void setActiveTypeAndIdx(SectionType type, uint32_t idx) {
	activeType = type;
	activeBankIdx = idx;
	if (curAddr[activeType].size() <= activeBankIdx) {
		curAddr[activeType].resize(activeBankIdx + 1, sectionTypeInfo[type].startAddr);
	}
}

static void setSectionType(SectionType type) {
	auto const &context = lexerStack.back();

	if (nbbanks(type) != 1) {
		scriptError(context, "A bank number must be specified for %s",
		            sectionTypeInfo[type].name.c_str());
		// Keep going with a default value for the bank index.
	}

	setActiveTypeAndIdx(type, 0); // There is only a single bank anyway, so just set the index to 0.
}

static void setSectionType(SectionType type, uint32_t bank) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[type];

	if (bank < typeInfo.firstBank) {
		scriptError(context, "%s bank %" PRIu32 " doesn't exist, the minimum is %" PRIu32,
		            typeInfo.name.c_str(), bank, typeInfo.firstBank);
		bank = typeInfo.firstBank;
	} else if (bank > typeInfo.lastBank) {
		scriptError(context, "%s bank %" PRIu32 " doesn't exist, the maximum is %" PRIu32,
		            typeInfo.name.c_str(), bank, typeInfo.lastBank);
	}

	setActiveTypeAndIdx(type, bank - typeInfo.firstBank);
}

static void setAddr(uint32_t addr) {
	auto const &context = lexerStack.back();
	auto &pc = curAddr[activeType][activeBankIdx];
	auto const &typeInfo = sectionTypeInfo[activeType];

	if (addr < pc) {
		scriptError(context, "ORG cannot be used to go backwards (from $%04x to $%04x)", pc, addr);
	} else if (addr > endaddr(activeType)) { // Allow "one past the end" sections.
		scriptError(context, "Cannot go to $%04" PRIx32 ": %s ends at $%04" PRIx16 "",
		            addr, typeInfo.name.c_str(), endaddr(activeType));
		pc = endaddr(activeType);
	} else {
		pc = addr;
	}
}

static void alignTo(uint32_t alignment, uint32_t alignOfs) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[activeType];
	auto &pc = curAddr[activeType][activeBankIdx];

	// TODO: maybe warn if truncating?
	alignOfs %= 1 << alignment;

	assert(pc >= typeInfo.startAddr);
	uint16_t length = alignment < 16 ? (uint16_t)(alignOfs - pc) % (1u << alignment)
	                                 : alignOfs - pc; // Let it wrap around, this'll trip the check.
	if (uint16_t offset = pc - typeInfo.startAddr; length > typeInfo.size - offset) {
		scriptError(context, "Cannot align: the next suitable address after $%04" PRIx16 " is $%04" PRIx16 ", past $%04" PRIx16,
		            pc, (uint16_t)(pc + length), (uint16_t)(endaddr(activeType) + 1));
	} else {
		pc += length;
	}
}

static void pad(uint32_t length) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[activeType];
	auto &pc = curAddr[activeType][activeBankIdx];

	assert(pc >= typeInfo.startAddr);
	if (uint16_t offset = pc - typeInfo.startAddr; length + offset > typeInfo.size) {
		scriptError(context, "Cannot pad by %u bytes: only %u bytes to $%04" PRIx16,
		            length, typeInfo.size - offset, (uint16_t)(endaddr(activeType) + 1));
	} else {
		pc += length;
	}
}

static void placeSection(std::string const &name) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[activeType];

	// A type *must* be active.
	if (activeType == SECTTYPE_INVALID) {
		scriptError(context, "No memory region has been specified to place section \"%s\" in",
		            name.c_str());
		return;
	}

	auto *section = sect_GetSection(name.c_str());
	if (!section) {
		scriptError(context, "Unknown section \"%s\"", name.c_str());
		return;
	}

	assert(section->offset == 0);
	// Check that the linker script doesn't contradict what the code says.
	if (section->type == SECTTYPE_INVALID) {
		// SDCC areas don't have a type assigned yet, so the linker script is used to give them one.
		for (Section *fragment = section; fragment; fragment = fragment->nextu) {
			fragment->type = activeType;
		}
	} else if (section->type != activeType) {
		scriptError(context, "\"%s\" is specified to be a %s section, but it is already a %s section",
		            name.c_str(), typeInfo.name.c_str(), sectionTypeInfo[section->type].name.c_str());
	}

	uint32_t bank = activeBankIdx + typeInfo.firstBank;
	if (section->isBankFixed && bank != section->bank) {
		scriptError(context, "Linker script wants section \"%s\" to go to %s bank %" PRIu32 ", but it is already in bank %" PRIu32,
		            name.c_str(), sectionTypeInfo[section->type].name.c_str(), bank, section->bank);
	}
	section->isBankFixed = true;
	section->bank = bank;

	uint16_t &org = curAddr[activeType][activeBankIdx];
	if (section->isAddressFixed && org != section->org) {
		scriptError(context, "Linker script wants section \"%s\" to go to address $%04" PRIx16 ", but it is already at $%04" PRIx16,
		            name.c_str(), org, section->org);
	} else if (section->isAlignFixed && (org & section->alignMask) != section->alignOfs) {
		uint8_t alignment = std::countr_one(section->alignMask);
		scriptError(context, "Linker script wants section \"%s\" to go to address $%04" PRIx16 ", but that would be ALIGN[%" PRIu8 ", %" PRIu16 "] instead of the ALIGN[%" PRIu8 ", %" PRIu16 "] it requests",
		            name.c_str(), org, alignment, (uint16_t)(org & section->alignMask), alignment, section->alignOfs);
	}
	section->isAddressFixed = true;
	section->isAlignFixed = false; // This can't be set when the above is.
	section->org = org;

	uint16_t curOfs = org - typeInfo.startAddr;
	if (section->size > typeInfo.size - curOfs) {
		scriptError(context, "Linker script wants section \"%s\" to go to address $%04" PRIx16 ", but then it would overflow %s by %" PRIx16 " bytes",
		            name.c_str(), org, typeInfo.name.c_str(),
		            (uint16_t)(section->size - (typeInfo.size - curOfs)));
		// Fill as much as possible without going out of bounds.
		org = typeInfo.startAddr + typeInfo.size;
	} else {
		org += section->size;
	}
}

// External API.

void script_ProcessScript(char const *path) {
	activeType = SECTTYPE_INVALID;

	lexerStack.clear();
	auto &newContext = lexerStack.emplace_back(std::string(path));

	if (!newContext.file.open(newContext.path, std::ios_base::in)) {
		error(NULL, 0, "Could not open linker script \"%s\"", newContext.path.c_str());
		lexerStack.clear();
	} else {
		yy::parser linkerScriptParser;
		// We don't care about the return value, as any error increments the global error count,
		// which is what `main` checks.
		(void)linkerScriptParser.parse();

		// Free up working memory.
		for (auto &region : curAddr) {
			region.clear();
		}
	}
}
