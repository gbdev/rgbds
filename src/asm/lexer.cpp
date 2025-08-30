// SPDX-License-Identifier: MIT

#include "asm/lexer.hpp"
#include <sys/stat.h>

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <inttypes.h>
#include <ios>
#include <limits.h>
#include <math.h>
#include <memory>
#include <new> // nothrow
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "helpers.hpp"
#include "platform.hpp"
#include "style.hpp"
#include "util.hpp"
#include "verbosity.hpp"

#include "asm/format.hpp"
#include "asm/fstack.hpp"
#include "asm/macro.hpp"
#include "asm/main.hpp"
#include "asm/rpn.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"
// Include this last so it gets all type & constant definitions
#include "parser.hpp" // For token definitions, generated from parser.y

// Bison 3.6 changed token "types" to "kinds"; cast to int for simple compatibility
#define T_(name) static_cast<int>(yy::parser::token::name)

struct Token {
	int type;
	std::variant<std::monostate, uint32_t, std::string> value;

	Token() : type(T_(NUMBER)), value(std::monostate{}) {}
	Token(int type_) : type(type_), value(std::monostate{}) {}
	Token(int type_, uint32_t value_) : type(type_), value(value_) {}
	Token(int type_, std::string const &value_) : type(type_), value(value_) {}
	Token(int type_, std::string &&value_) : type(type_), value(value_) {}
};

// This map lists all RGBASM keywords which `yylex_NORMAL` lexes as identifiers.
// All non-identifier tokens are lexed separately.
static UpperMap<int> const keywordDict{
    {"ADC",           T_(SM83_ADC)         },
    {"ADD",           T_(SM83_ADD)         },
    {"AND",           T_(SM83_AND)         },
    {"BIT",           T_(SM83_BIT)         },
    {"CALL",          T_(SM83_CALL)        },
    {"CCF",           T_(SM83_CCF)         },
    {"CPL",           T_(SM83_CPL)         },
    {"CP",            T_(SM83_CP)          },
    {"DAA",           T_(SM83_DAA)         },
    {"DEC",           T_(SM83_DEC)         },
    {"DI",            T_(SM83_DI)          },
    {"EI",            T_(SM83_EI)          },
    {"HALT",          T_(SM83_HALT)        },
    {"INC",           T_(SM83_INC)         },
    {"JP",            T_(SM83_JP)          },
    {"JR",            T_(SM83_JR)          },
    {"LD",            T_(SM83_LD)          },
    {"LDI",           T_(SM83_LDI)         },
    {"LDD",           T_(SM83_LDD)         },
    {"LDH",           T_(SM83_LDH)         },
    {"NOP",           T_(SM83_NOP)         },
    {"OR",            T_(SM83_OR)          },
    {"POP",           T_(SM83_POP)         },
    {"PUSH",          T_(SM83_PUSH)        },
    {"RES",           T_(SM83_RES)         },
    {"RETI",          T_(SM83_RETI)        },
    {"RET",           T_(SM83_RET)         },
    {"RLCA",          T_(SM83_RLCA)        },
    {"RLC",           T_(SM83_RLC)         },
    {"RLA",           T_(SM83_RLA)         },
    {"RL",            T_(SM83_RL)          },
    {"RRC",           T_(SM83_RRC)         },
    {"RRCA",          T_(SM83_RRCA)        },
    {"RRA",           T_(SM83_RRA)         },
    {"RR",            T_(SM83_RR)          },
    {"RST",           T_(SM83_RST)         },
    {"SBC",           T_(SM83_SBC)         },
    {"SCF",           T_(SM83_SCF)         },
    {"SET",           T_(SM83_SET)         },
    {"SLA",           T_(SM83_SLA)         },
    {"SRA",           T_(SM83_SRA)         },
    {"SRL",           T_(SM83_SRL)         },
    {"STOP",          T_(SM83_STOP)        },
    {"SUB",           T_(SM83_SUB)         },
    {"SWAP",          T_(SM83_SWAP)        },
    {"XOR",           T_(SM83_XOR)         },

    {"NZ",            T_(CC_NZ)            },
    {"Z",             T_(CC_Z)             },
    {"NC",            T_(CC_NC)            },
    // There is no `T_(CC_C)`; it's handled before as `T_(TOKEN_C)`

    {"AF",            T_(MODE_AF)          },
    {"BC",            T_(MODE_BC)          },
    {"DE",            T_(MODE_DE)          },
    {"HL",            T_(MODE_HL)          },
    {"SP",            T_(MODE_SP)          },
    {"HLD",           T_(MODE_HL_DEC)      },
    {"HLI",           T_(MODE_HL_INC)      },

    {"A",             T_(TOKEN_A)          },
    {"B",             T_(TOKEN_B)          },
    {"C",             T_(TOKEN_C)          },
    {"D",             T_(TOKEN_D)          },
    {"E",             T_(TOKEN_E)          },
    {"H",             T_(TOKEN_H)          },
    {"L",             T_(TOKEN_L)          },

    {"DEF",           T_(OP_DEF)           },

    {"FRAGMENT",      T_(POP_FRAGMENT)     },
    {"BANK",          T_(OP_BANK)          },
    {"ALIGN",         T_(POP_ALIGN)        },

    {"SIZEOF",        T_(OP_SIZEOF)        },
    {"STARTOF",       T_(OP_STARTOF)       },

    {"ROUND",         T_(OP_ROUND)         },
    {"CEIL",          T_(OP_CEIL)          },
    {"FLOOR",         T_(OP_FLOOR)         },
    {"DIV",           T_(OP_FDIV)          },
    {"MUL",           T_(OP_FMUL)          },
    {"FMOD",          T_(OP_FMOD)          },
    {"POW",           T_(OP_POW)           },
    {"LOG",           T_(OP_LOG)           },
    {"SIN",           T_(OP_SIN)           },
    {"COS",           T_(OP_COS)           },
    {"TAN",           T_(OP_TAN)           },
    {"ASIN",          T_(OP_ASIN)          },
    {"ACOS",          T_(OP_ACOS)          },
    {"ATAN",          T_(OP_ATAN)          },
    {"ATAN2",         T_(OP_ATAN2)         },

    {"HIGH",          T_(OP_HIGH)          },
    {"LOW",           T_(OP_LOW)           },
    {"ISCONST",       T_(OP_ISCONST)       },

    {"BITWIDTH",      T_(OP_BITWIDTH)      },
    {"TZCOUNT",       T_(OP_TZCOUNT)       },

    {"BYTELEN",       T_(OP_BYTELEN)       },
    {"READFILE",      T_(OP_READFILE)      },
    {"STRBYTE",       T_(OP_STRBYTE)       },
    {"STRCAT",        T_(OP_STRCAT)        },
    {"STRCHAR",       T_(OP_STRCHAR)       },
    {"STRCMP",        T_(OP_STRCMP)        },
    {"STRFIND",       T_(OP_STRFIND)       },
    {"STRFMT",        T_(OP_STRFMT)        },
    {"STRIN",         T_(OP_STRIN)         },
    {"STRLEN",        T_(OP_STRLEN)        },
    {"STRLWR",        T_(OP_STRLWR)        },
    {"STRRFIND",      T_(OP_STRRFIND)      },
    {"STRRIN",        T_(OP_STRRIN)        },
    {"STRRPL",        T_(OP_STRRPL)        },
    {"STRSLICE",      T_(OP_STRSLICE)      },
    {"STRSUB",        T_(OP_STRSUB)        },
    {"STRUPR",        T_(OP_STRUPR)        },

    {"CHARCMP",       T_(OP_CHARCMP)       },
    {"CHARLEN",       T_(OP_CHARLEN)       },
    {"CHARSIZE",      T_(OP_CHARSIZE)      },
    {"CHARSUB",       T_(OP_CHARSUB)       },
    {"CHARVAL",       T_(OP_CHARVAL)       },
    {"INCHARMAP",     T_(OP_INCHARMAP)     },
    {"REVCHAR",       T_(OP_REVCHAR)       },

    {"INCLUDE",       T_(POP_INCLUDE)      },
    {"PRINT",         T_(POP_PRINT)        },
    {"PRINTLN",       T_(POP_PRINTLN)      },
    {"EXPORT",        T_(POP_EXPORT)       },
    {"DS",            T_(POP_DS)           },
    {"DB",            T_(POP_DB)           },
    {"DW",            T_(POP_DW)           },
    {"DL",            T_(POP_DL)           },
    {"SECTION",       T_(POP_SECTION)      },
    {"ENDSECTION",    T_(POP_ENDSECTION)   },
    {"PURGE",         T_(POP_PURGE)        },

    {"RSRESET",       T_(POP_RSRESET)      },
    {"RSSET",         T_(POP_RSSET)        },

    {"INCBIN",        T_(POP_INCBIN)       },
    {"CHARMAP",       T_(POP_CHARMAP)      },
    {"NEWCHARMAP",    T_(POP_NEWCHARMAP)   },
    {"SETCHARMAP",    T_(POP_SETCHARMAP)   },
    {"PUSHC",         T_(POP_PUSHC)        },
    {"POPC",          T_(POP_POPC)         },

    {"FAIL",          T_(POP_FAIL)         },
    {"WARN",          T_(POP_WARN)         },
    {"FATAL",         T_(POP_FATAL)        },
    {"ASSERT",        T_(POP_ASSERT)       },
    {"STATIC_ASSERT", T_(POP_STATIC_ASSERT)},

    {"MACRO",         T_(POP_MACRO)        },
    {"ENDM",          T_(POP_ENDM)         },
    {"SHIFT",         T_(POP_SHIFT)        },

    {"REPT",          T_(POP_REPT)         },
    {"FOR",           T_(POP_FOR)          },
    {"ENDR",          T_(POP_ENDR)         },
    {"BREAK",         T_(POP_BREAK)        },

    {"LOAD",          T_(POP_LOAD)         },
    {"ENDL",          T_(POP_ENDL)         },

    {"IF",            T_(POP_IF)           },
    {"ELSE",          T_(POP_ELSE)         },
    {"ELIF",          T_(POP_ELIF)         },
    {"ENDC",          T_(POP_ENDC)         },

    {"UNION",         T_(POP_UNION)        },
    {"NEXTU",         T_(POP_NEXTU)        },
    {"ENDU",          T_(POP_ENDU)         },

    {"WRAM0",         T_(SECT_WRAM0)       },
    {"VRAM",          T_(SECT_VRAM)        },
    {"ROMX",          T_(SECT_ROMX)        },
    {"ROM0",          T_(SECT_ROM0)        },
    {"HRAM",          T_(SECT_HRAM)        },
    {"WRAMX",         T_(SECT_WRAMX)       },
    {"SRAM",          T_(SECT_SRAM)        },
    {"OAM",           T_(SECT_OAM)         },

    {"RB",            T_(POP_RB)           },
    {"RW",            T_(POP_RW)           },
    // There is no `T_(POP_RL)`; it's handled before as `T_(SM83_RL)`

    {"EQU",           T_(POP_EQU)          },
    {"EQUS",          T_(POP_EQUS)         },
    {"REDEF",         T_(POP_REDEF)        },

    {"PUSHS",         T_(POP_PUSHS)        },
    {"POPS",          T_(POP_POPS)         },
    {"PUSHO",         T_(POP_PUSHO)        },
    {"POPO",          T_(POP_POPO)         },

    {"OPT",           T_(POP_OPT)          },
};

static LexerState *lexerState = nullptr;
static LexerState *lexerStateEOL = nullptr;

bool lexer_AtTopLevel() {
	return lexerState == nullptr;
}

void LexerState::clear(uint32_t lineNo_) {
	mode = LEXER_NORMAL;
	atLineStart = true;
	lastToken = T_(YYEOF);
	nextToken = 0;

	ifStack.clear();

	capturing = false;
	captureBuf = nullptr;

	disableExpansions = false;
	expansionScanDistance = 0;
	expandStrings = true;

	expansions.clear();

	lineNo = lineNo_; // Will be incremented at next line start
}

static void nextLine() {
	// Newlines read within an expansion should not increase the line count
	if (lexerState->expansions.empty()) {
		++lexerState->lineNo;
	}
}

uint32_t lexer_GetIFDepth() {
	return lexerState->ifStack.size();
}

void lexer_IncIFDepth() {
	lexerState->ifStack.push_front({.ranIfBlock = false, .reachedElseBlock = false});
}

void lexer_DecIFDepth() {
	if (lexerState->ifStack.empty()) {
		fatal("Found `ENDC` outside of a conditional (not after an `IF`/`ELIF`/`ELSE` block)");
	}

	lexerState->ifStack.pop_front();
}

bool lexer_RanIFBlock() {
	return lexerState->ifStack.front().ranIfBlock;
}

bool lexer_ReachedELSEBlock() {
	return lexerState->ifStack.front().reachedElseBlock;
}

void lexer_RunIFBlock() {
	lexerState->ifStack.front().ranIfBlock = true;
}

void lexer_ReachELSEBlock() {
	lexerState->ifStack.front().reachedElseBlock = true;
}

void LexerState::setAsCurrentState() {
	lexerState = this;
}

void LexerState::setFileAsNextState(std::string const &filePath, bool updateStateNow) {
	if (filePath == "-") {
		path = "<stdin>";
		content.emplace<BufferedContent>(STDIN_FILENO);
		verbosePrint(VERB_INFO, "Opening stdin\n"); // LCOV_EXCL_LINE
	} else {
		struct stat statBuf;
		if (stat(filePath.c_str(), &statBuf) != 0) {
			// LCOV_EXCL_START
			fatal("Failed to stat file \"%s\": %s", filePath.c_str(), strerror(errno));
			// LCOV_EXCL_STOP
		}
		path = filePath;

		if (size_t size = static_cast<size_t>(statBuf.st_size); statBuf.st_size > 0) {
			// Read the entire file for better performance
			// Ideally we'd use C++20 `auto ptr = std::make_shared<char[]>(size)`,
			// but it has insufficient compiler support
			auto ptr = std::shared_ptr<char[]>(new (std::nothrow) char[size]);

			if (std::ifstream fs(path, std::ios::binary); !fs) {
				// LCOV_EXCL_START
				fatal("Failed to open file \"%s\": %s", path.c_str(), strerror(errno));
				// LCOV_EXCL_STOP
			} else if (!fs.read(ptr.get(), size)) {
				// LCOV_EXCL_START
				fatal("Failed to read file \"%s\": %s", path.c_str(), strerror(errno));
				// LCOV_EXCL_STOP
			}
			content.emplace<ViewedContent>(ptr, size);

			// LCOV_EXCL_START
			verbosePrint(VERB_INFO, "File \"%s\" is fully read\n", path.c_str());
			// LCOV_EXCL_STOP
		} else {
			// LCOV_EXCL_START
			if (statBuf.st_size == 0) {
				verbosePrint(VERB_INFO, "File \"%s\" is empty\n", path.c_str());
			} else {
				verbosePrint(
				    VERB_INFO, "Failed to stat file \"%s\": %s\n", path.c_str(), strerror(errno)
				);
			}
			// LCOV_EXCL_STOP

			// Have a fallback if reading the file failed
			int fd = open(path.c_str(), O_RDONLY);
			if (fd < 0) {
				// LCOV_EXCL_START
				fatal("Failed to open file \"%s\": %s", path.c_str(), strerror(errno));
				// LCOV_EXCL_STOP
			}
			content.emplace<BufferedContent>(fd);

			verbosePrint(VERB_INFO, "File \"%s\" is opened\n", path.c_str()); // LCOV_EXCL_LINE
		}
	}

	clear(0);
	if (updateStateNow) {
		lexerState = this;
	} else {
		lexerStateEOL = this;
	}
}

void LexerState::setViewAsNextState(char const *name, ContentSpan const &span, uint32_t lineNo_) {
	path = name; // Used to report read errors in `.peek()`
	content.emplace<ViewedContent>(span);
	clear(lineNo_);
	lexerStateEOL = this;
}

void lexer_RestartRept(uint32_t lineNo) {
	if (std::holds_alternative<ViewedContent>(lexerState->content)) {
		std::get<ViewedContent>(lexerState->content).offset = 0;
	}
	lexerState->clear(lineNo);
}

LexerState::~LexerState() {
	// A big chunk of the lexer state soundness is the file stack ("fstack").
	// Each context in the fstack has its own *unique* lexer state; thus, we always guarantee
	// that lexer states lifetimes are always properly managed, since they're handled solely
	// by the fstack... with *one* exception.
	// Assume a context is pushed on top of the fstack, and the corresponding lexer state gets
	// scheduled at EOF; `lexerStateEOL` thus becomes a (weak) ref to that lexer state...
	// It has been possible, due to a bug, that the corresponding fstack context gets popped
	// before EOL, deleting the associated state... but it would still be switched to at EOL.
	// This assumption checks that this doesn't happen again.
	// It could be argued that deleting a state that's scheduled for EOF could simply clear
	// `lexerStateEOL`, but there's currently no situation in which this should happen.
	assume(this != lexerStateEOL);
}

bool Expansion::advance() {
	assume(offset <= size());
	return ++offset > size();
}

BufferedContent::~BufferedContent() {
	close(fd);
}

void BufferedContent::advance() {
	assume(offset < std::size(buf));
	if (++offset == std::size(buf)) {
		offset = 0; // Wrap around if necessary
	}
	if (size > 0) {
		--size;
	}
}

void BufferedContent::refill() {
	size_t target = std::size(buf) - size; // Aim: making the buf full

	// Compute the index we'll start writing to
	size_t startIndex = (offset + size) % std::size(buf);

	// If the range to fill passes over the buffer wrapping point, we need two reads
	if (startIndex + target > std::size(buf)) {
		size_t nbExpectedChars = std::size(buf) - startIndex;
		size_t nbReadChars = readMore(startIndex, nbExpectedChars);

		startIndex += nbReadChars;
		if (startIndex == std::size(buf)) {
			startIndex = 0;
		}

		// If the read was incomplete, don't perform a second read
		target -= nbReadChars;
		if (nbReadChars < nbExpectedChars) {
			target = 0;
		}
	}
	if (target != 0) {
		readMore(startIndex, target);
	}
}

size_t BufferedContent::readMore(size_t startIndex, size_t nbChars) {
	// This buffer overflow made me lose WEEKS of my life. Never again.
	assume(startIndex + nbChars <= std::size(buf));
	ssize_t nbReadChars = read(fd, &buf[startIndex], nbChars);

	if (nbReadChars == -1) {
		// LCOV_EXCL_START
		fatal("Error reading file \"%s\": %s", lexerState->path.c_str(), strerror(errno));
		// LCOV_EXCL_STOP
	}

	size += nbReadChars;

	// `nbReadChars` cannot be negative, so it's fine to cast to `size_t`
	return static_cast<size_t>(nbReadChars);
}

void lexer_SetMode(LexerMode mode) {
	lexerState->mode = mode;
}

void lexer_ToggleStringExpansion(bool enable) {
	lexerState->expandStrings = enable;
}

// Functions for the actual lexer to obtain characters

static void beginExpansion(std::shared_ptr<std::string> str, std::optional<std::string> name) {
	if (name) {
		lexer_CheckRecursionDepth();
	}

	// Do not expand empty strings
	if (str->empty()) {
		return;
	}

	lexerState->expansions.push_front({.name = name, .contents = str, .offset = 0});
}

void lexer_CheckRecursionDepth() {
	if (lexerState->expansions.size() > options.maxRecursionDepth + 1) {
		fatal("Recursion limit (%zu) exceeded", options.maxRecursionDepth);
	}
}

static bool isMacroChar(char c) {
	return c == '@' || c == '#' || c == '<' || (c >= '1' && c <= '9');
}

// Forward declarations for `readBracketedMacroArgNum`
static int peek();
static void shiftChar();
static int bumpChar();
static int nextChar();
static uint32_t readDecimalNumber(int initial);

static uint32_t readBracketedMacroArgNum() {
	bool disableExpansions = lexerState->disableExpansions;
	lexerState->disableExpansions = false;
	Defer restoreExpansions{[&] { lexerState->disableExpansions = disableExpansions; }};

	int32_t num = 0;
	int c = peek();
	bool empty = false;
	bool symbolError = false;
	bool negative = c == '-';

	if (negative) {
		c = nextChar();
	}

	if (isDigit(c)) {
		uint32_t n = readDecimalNumber(bumpChar());
		if (n > INT32_MAX) {
			error("Number in bracketed macro argument is too large");
			return 0;
		}
		num = negative ? -n : static_cast<int32_t>(n);
	} else if (startsIdentifier(c) || c == '#') {
		if (c == '#') {
			c = nextChar();
			if (!startsIdentifier(c)) {
				error("Empty raw symbol in bracketed macro argument");
				return 0;
			}
		}

		std::string symName;
		for (; continuesIdentifier(c); c = nextChar()) {
			symName += c;
		}

		if (Symbol const *sym = sym_FindScopedValidSymbol(symName); !sym) {
			if (sym_IsPurgedScoped(symName)) {
				error("Bracketed symbol `%s` does not exist; it was purged", symName.c_str());
			} else {
				error("Bracketed symbol `%s` does not exist", symName.c_str());
			}
			num = 0;
			symbolError = true;
		} else if (!sym->isNumeric()) {
			error("Bracketed symbol `%s` is not numeric", symName.c_str());
			num = 0;
			symbolError = true;
		} else {
			num = static_cast<int32_t>(sym->getConstantValue());
		}
	} else {
		empty = true;
	}

	c = bumpChar();
	if (c != '>') {
		error("Invalid character %s in bracketed macro argument", printChar(c));
		return 0;
	} else if (empty) {
		error("Empty bracketed macro argument");
		return 0;
	} else if (num == 0 && !symbolError) {
		error("Invalid bracketed macro argument \"\\<0>\"");
		return 0;
	} else {
		return num;
	}
}

static std::shared_ptr<std::string> readMacroArg() {
	if (int c = bumpChar(); c == '@') {
		std::shared_ptr<std::string> str = fstk_GetUniqueIDStr();
		if (!str) {
			error("`\\@` cannot be used outside of a macro or loop (`REPT`/`FOR` block)");
		}
		return str;
	} else if (c == '#') {
		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		if (!macroArgs) {
			error("`\\#` cannot be used outside of a macro");
			return nullptr;
		}

		std::shared_ptr<std::string> str = macroArgs->getAllArgs();
		assume(str); // '\#' should always be defined (at least as an empty string)
		return str;
	} else if (c == '<') {
		int32_t num = readBracketedMacroArgNum();
		if (num == 0) {
			// The error was already reported by `readBracketedMacroArgNum`.
			return nullptr;
		}

		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		if (!macroArgs) {
			error("`\\<%" PRIu32 ">` cannot be used outside of a macro", num);
			return nullptr;
		}

		std::shared_ptr<std::string> str = macroArgs->getArg(num);
		if (!str) {
			error("Macro argument `\\<%" PRId32 ">` not defined", num);
		}
		return str;
	} else {
		assume(c >= '1' && c <= '9');

		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		if (!macroArgs) {
			error("`\\%c` cannot be used outside of a macro", c);
			return nullptr;
		}

		std::shared_ptr<std::string> str = macroArgs->getArg(c - '0');
		if (!str) {
			error("Macro argument `\\%c` not defined", c);
		}
		return str;
	}
}

int LexerState::peekChar() {
	// This is `.peekCharAhead()` modified for zero lookahead distance
	for (Expansion &exp : expansions) {
		if (exp.offset < exp.size()) {
			return static_cast<uint8_t>((*exp.contents)[exp.offset]);
		}
	}

	if (std::holds_alternative<ViewedContent>(content)) {
		auto &view = std::get<ViewedContent>(content);
		if (view.offset < view.span.size) {
			return static_cast<uint8_t>(view.span.ptr[view.offset]);
		}
	} else {
		auto &cbuf = std::get<BufferedContent>(content);
		if (cbuf.size == 0) {
			cbuf.refill();
		}
		assume(cbuf.offset < std::size(cbuf.buf));
		if (cbuf.size > 0) {
			return static_cast<uint8_t>(cbuf.buf[cbuf.offset]);
		}
	}

	// If there aren't enough chars, give up
	return EOF;
}

int LexerState::peekCharAhead() {
	// We only need one character of lookahead, for macro arguments
	uint8_t distance = 1;

	for (Expansion &exp : expansions) {
		// An expansion that has reached its end will have `exp.offset` == `exp.size()`,
		// and `.peekCharAhead()` will continue with its parent
		assume(exp.offset <= exp.size());
		if (size_t idx = exp.offset + distance; idx < exp.size()) {
			// Macro args can't be recursive, since `peek()` marks them as scanned, so
			// this is a failsafe that (as far as I can tell) won't ever actually run.
			return static_cast<uint8_t>((*exp.contents)[idx]); // LCOV_EXCL_LINE
		}
		distance -= exp.size() - exp.offset;
	}

	if (std::holds_alternative<ViewedContent>(content)) {
		auto &view = std::get<ViewedContent>(content);
		if (view.offset + distance < view.span.size) {
			return static_cast<uint8_t>(view.span.ptr[view.offset + distance]);
		}
	} else {
		auto &cbuf = std::get<BufferedContent>(content);
		assume(distance < std::size(cbuf.buf));
		if (cbuf.size <= distance) {
			cbuf.refill();
		}
		if (cbuf.size > distance) {
			return static_cast<uint8_t>(cbuf.buf[(cbuf.offset + distance) % std::size(cbuf.buf)]);
		}
	}

	// If there aren't enough chars, give up
	return EOF;
}

// Forward declarations for `peek`
static std::pair<Symbol const *, std::shared_ptr<std::string>> readInterpolation(size_t depth);

static int peek() {
	int c = lexerState->peekChar();

	if (lexerState->expansionScanDistance > 0) {
		return c;
	}

	++lexerState->expansionScanDistance; // Do not consider again

	if (lexerState->disableExpansions) {
		return c;
	} else if (c == '\\') {
		// If character is a backslash, check for a macro arg
		++lexerState->expansionScanDistance;
		if (!isMacroChar(lexerState->peekCharAhead())) {
			return c;
		}

		// If character is a macro arg char, do macro arg expansion
		shiftChar();
		if (std::shared_ptr<std::string> str = readMacroArg(); str) {
			beginExpansion(str, std::nullopt);

			// Mark the entire macro arg expansion as "painted blue"
			// so that macro args can't be recursive
			// https://en.wikipedia.org/wiki/Painted_blue
			lexerState->expansionScanDistance += str->length();
		}

		return peek(); // Tail recursion
	} else if (c == '{') {
		// If character is an open brace, do symbol interpolation
		shiftChar();
		if (auto interp = readInterpolation(0); interp.first && interp.second) {
			beginExpansion(interp.second, interp.first->name);
		}

		return peek(); // Tail recursion
	} else {
		return c;
	}
}

static void shiftChar() {
	if (lexerState->capturing) {
		if (lexerState->captureBuf) {
			int c = peek();
			assume(c != EOF); // Avoid calling `shiftChar()` when it could be EOF while capturing
			lexerState->captureBuf->push_back(c);
		}
		++lexerState->captureSize;
	}

	--lexerState->expansionScanDistance;

	for (;;) {
		if (!lexerState->expansions.empty()) {
			// Advance within the current expansion
			if (Expansion &exp = lexerState->expansions.front(); exp.advance()) {
				// When advancing would go past an expansion's end,
				// move up to its parent and try again to advance
				lexerState->expansions.pop_front();
				continue;
			}
		} else {
			// Advance within the file contents
			if (std::holds_alternative<ViewedContent>(lexerState->content)) {
				++std::get<ViewedContent>(lexerState->content).offset;
			} else {
				std::get<BufferedContent>(lexerState->content).advance();
			}
		}
		return;
	}
}

static bool consumeChar(int c) {
	// This is meant to be called when the "extra" behavior of `peek()` is not wanted,
	// e.g. painting the peeked-at character "blue".
	if (lexerState->peekChar() != c) {
		return false;
	}

	// Increment `lexerState->expansionScanDistance` to prevent `shiftChar()` from calling
	// `peek()` and to balance its decrement.
	++lexerState->expansionScanDistance;
	shiftChar();
	return true;
}

static int bumpChar() {
	int c = peek();
	shiftChar();
	return c;
}

static int nextChar() {
	shiftChar();
	return peek();
}

template<typename P>
static int skipChars(P predicate) {
	int c = peek();
	while (predicate(c)) {
		c = nextChar();
	}
	return c;
}

static void handleCRLF(int c) {
	if (c == '\r' && peek() == '\n') {
		shiftChar();
	}
}

static auto scopedDisableExpansions() {
	lexerState->disableExpansions = true;
	return Defer{[&] { lexerState->disableExpansions = false; }};
}

// "Services" provided by the lexer to the rest of the program

uint32_t lexer_GetLineNo() {
	return lexerState->lineNo;
}

void lexer_TraceStringExpansions() {
	if (!lexerState) {
		return;
	}

	for (Expansion &exp : lexerState->expansions) {
		// Only print EQUS expansions, not string args
		if (exp.name) {
			style_Set(stderr, STYLE_CYAN, false);
			fputs("    while expanding symbol `", stderr);
			style_Set(stderr, STYLE_CYAN, true);
			fputs(exp.name->c_str(), stderr);
			style_Set(stderr, STYLE_CYAN, false);
			fputs("`\n", stderr);
		}
	}
	style_Reset(stderr);
}

// Functions to discard non-tokenized characters

static void discardBlockComment() {
	Defer reenableExpansions = scopedDisableExpansions();
	for (;;) {
		int c = bumpChar();

		switch (c) {
		case EOF:
			error("Unterminated block comment");
			return;
		case '\r':
			handleCRLF(c);
			[[fallthrough]];
		case '\n':
			nextLine();
			continue;
		case '/':
			if (peek() == '*') {
				warning(WARNING_NESTED_COMMENT, "\"/*\" in block comment");
			}
			continue;
		case '*':
			if (peek() == '/') {
				shiftChar();
				return;
			}
			[[fallthrough]];
		default:
			continue;
		}
	}
}

static void discardComment() {
	Defer reenableExpansions = scopedDisableExpansions();
	skipChars([](int c) { return c != EOF && !isNewline(c); });
}

static void discardLineContinuation() {
	for (;;) {
		if (int c = peek(); isBlankSpace(c)) {
			shiftChar();
		} else if (isNewline(c)) {
			shiftChar();
			handleCRLF(c);
			nextLine();
			break;
		} else if (c == ';') {
			discardComment();
		} else if (c == EOF) {
			error("Invalid line continuation at end of file");
			break;
		} else {
			error("Invalid character %s after line continuation", printChar(c));
			break;
		}
	}
}

// Functions to read tokenizable values

static std::string readAnonLabelRef(char c) {
	// We come here having already peeked at one char, so no need to do it again
	uint32_t n = 1;
	while (nextChar() == c) {
		++n;
	}
	return sym_MakeAnonLabelName(n, c == '-');
}

static uint32_t readFractionalPart(uint32_t integer) {
	uint32_t value = 0, divisor = 1;
	uint8_t precision = 0;
	enum {
		READFRACTIONALPART_DIGITS,
		READFRACTIONALPART_PRECISION,
		READFRACTIONALPART_PRECISION_DIGITS,
	} state = READFRACTIONALPART_DIGITS;
	bool nonDigit = true;

	for (int c = peek();; c = nextChar()) {
		if (state == READFRACTIONALPART_DIGITS) {
			if (c == '_') {
				if (nonDigit) {
					error("Invalid integer constant, '_' after another '_'");
				}
				nonDigit = true;
				continue;
			}

			if (c == 'q' || c == 'Q') {
				state = READFRACTIONALPART_PRECISION;
				nonDigit = false; // '_' is allowed before 'q'/'Q'
				continue;
			} else if (!isDigit(c)) {
				break;
			}
			nonDigit = false;

			if (divisor > (UINT32_MAX - (c - '0')) / 10) {
				warning(WARNING_LARGE_CONSTANT, "Precision of fixed-point constant is too large");
				// Discard any additional digits
				skipChars([](int d) { return isDigit(d) || d == '_'; });
				break;
			}
			value = value * 10 + (c - '0');
			divisor *= 10;
		} else {
			if (c == '.' && state == READFRACTIONALPART_PRECISION) {
				state = READFRACTIONALPART_PRECISION_DIGITS;
				continue;
			} else if (!isDigit(c)) {
				break;
			}

			precision = precision * 10 + (c - '0');
		}
	}

	if (precision == 0) {
		if (state >= READFRACTIONALPART_PRECISION) {
			error("Invalid fixed-point constant, no significant digits after 'q'");
		}
		precision = options.fixPrecision;
	} else if (precision > 31) {
		error("Fixed-point constant precision must be between 1 and 31");
		precision = options.fixPrecision;
	}
	if (nonDigit) {
		error("Invalid fixed-point constant, trailing '_'");
	}

	if (integer >= (1ULL << (32 - precision))) {
		warning(WARNING_LARGE_CONSTANT, "Magnitude of fixed-point constant is too large");
		return 0;
	}

	// Cast to unsigned avoids undefined overflow behavior
	uint32_t fractional =
	    static_cast<uint32_t>(round(static_cast<double>(value) / divisor * pow(2.0, precision)));

	return (integer << precision) | fractional;
}

static bool isValidDigit(char c) {
	return isAlphanumeric(c) || c == '.' || c == '#' || c == '@';
}

static bool checkDigitErrors(char const *digits, size_t n, char const *type) {
	for (size_t i = 0; i < n; ++i) {
		char c = digits[i];

		if (!isValidDigit(c)) {
			error("Invalid digit for %s constant %s", type, printChar(c));
			return false;
		}

		if (c >= '0' && c < static_cast<char>(n + '0') && c != static_cast<char>(i + '0')) {
			error("Changed digit for %s constant %s", type, printChar(c));
			return false;
		}

		for (size_t j = i + 1; j < n; ++j) {
			if (c == digits[j]) {
				error("Repeated digit for %s constant %s", type, printChar(c));
				return false;
			}
		}
	}

	return true;
}

void lexer_SetBinDigits(char const digits[2]) {
	if (size_t n = std::size(options.binDigits); checkDigitErrors(digits, n, "binary")) {
		memcpy(options.binDigits, digits, n);
	}
}

void lexer_SetGfxDigits(char const digits[4]) {
	if (size_t n = std::size(options.gfxDigits); checkDigitErrors(digits, n, "graphics")) {
		memcpy(options.gfxDigits, digits, n);
	}
}

static uint32_t readBinaryNumber(char const *prefix) {
	uint32_t value = 0;
	bool empty = true;
	bool nonDigit = false;

	for (int c = peek();; c = nextChar()) {
		if (c == '_') {
			if (nonDigit) {
				error("Invalid integer constant, '_' after another '_'");
			}
			nonDigit = true;
			continue;
		}

		int bit;
		if (c == '0' || c == options.binDigits[0]) {
			bit = 0;
		} else if (c == '1' || c == options.binDigits[1]) {
			bit = 1;
		} else {
			break;
		}
		empty = false;
		nonDigit = false;

		if (value > (UINT32_MAX - bit) / 2) {
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large");
			// Discard any additional digits
			skipChars([](int d) {
				return d == '0' || d == '1' || d == options.binDigits[0]
				       || d == options.binDigits[1] || d == '_';
			});
			return 0;
		}
		value = value * 2 + bit;
	}

	if (empty) {
		error("Invalid integer constant, no digits after %s", prefix);
	}
	if (nonDigit) {
		error("Invalid integer constant, trailing '_'");
	}

	return value;
}

static uint32_t readOctalNumber(char const *prefix) {
	uint32_t value = 0;
	bool empty = true;
	bool nonDigit = false;

	for (int c = peek();; c = nextChar()) {
		if (c == '_') {
			if (nonDigit) {
				error("Invalid integer constant, '_' after another '_'");
			}
			nonDigit = true;
			continue;
		}

		if (isOctDigit(c)) {
			c = c - '0';
		} else {
			break;
		}
		empty = false;
		nonDigit = false;

		if (value > (UINT32_MAX - c) / 8) {
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large");
			// Discard any additional digits
			skipChars([](int d) { return isOctDigit(d) || d == '_'; });
			return 0;
		}
		value = value * 8 + c;
	}

	if (empty) {
		error("Invalid integer constant, no digits after %s", prefix);
	}
	if (nonDigit) {
		error("Invalid integer constant, trailing '_'");
	}

	return value;
}

static uint32_t readDecimalNumber(int initial) {
	assume(isDigit(initial));
	uint32_t value = initial - '0';
	bool nonDigit = false;

	for (int c = peek();; c = nextChar()) {
		if (c == '_') {
			if (nonDigit) {
				error("Invalid integer constant, '_' after another '_'");
			}
			nonDigit = true;
			continue;
		}

		if (isDigit(c)) {
			c = c - '0';
		} else {
			break;
		}
		nonDigit = false;

		if (value > (UINT32_MAX - c) / 10) {
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large");
			// Discard any additional digits
			skipChars([](int d) { return isDigit(d) || d == '_'; });
			return 0;
		}
		value = value * 10 + c;
	}

	if (nonDigit) {
		error("Invalid integer constant, trailing '_'");
	}

	return value;
}

static uint32_t readHexNumber(char const *prefix) {
	uint32_t value = 0;
	bool empty = true;
	bool nonDigit = false;

	for (int c = peek();; c = nextChar()) {
		if (c == '_') {
			if (nonDigit) {
				error("Invalid integer constant, '_' after another '_'");
			}
			nonDigit = true;
			continue;
		}

		if (c >= 'a' && c <= 'f') {
			c = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			c = c - 'A' + 10;
		} else if (isDigit(c)) {
			c = c - '0';
		} else {
			break;
		}
		empty = false;
		nonDigit = false;

		if (value > (UINT32_MAX - c) / 16) {
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large");
			// Discard any additional digits
			skipChars([](int d) { return isHexDigit(d) || d == '_'; });
			return 0;
		}
		value = value * 16 + c;
	}

	if (empty) {
		error("Invalid integer constant, no digits after %s", prefix);
	}
	if (nonDigit) {
		error("Invalid integer constant, trailing '_'");
	}

	return value;
}

static uint32_t readGfxConstant() {
	uint32_t bitPlaneLower = 0, bitPlaneUpper = 0;
	uint8_t width = 0;
	bool nonDigit = false;

	for (int c = peek();; c = nextChar()) {
		if (c == '_') {
			if (nonDigit) {
				error("Invalid integer constant, '_' after another '_'");
			}
			nonDigit = true;
			continue;
		}

		uint32_t pixel;
		if (c == '0' || c == options.gfxDigits[0]) {
			pixel = 0;
		} else if (c == '1' || c == options.gfxDigits[1]) {
			pixel = 1;
		} else if (c == '2' || c == options.gfxDigits[2]) {
			pixel = 2;
		} else if (c == '3' || c == options.gfxDigits[3]) {
			pixel = 3;
		} else {
			break;
		}
		nonDigit = false;

		if (width < 8) {
			bitPlaneLower = bitPlaneLower << 1 | (pixel & 1);
			bitPlaneUpper = bitPlaneUpper << 1 | (pixel >> 1);
		}
		if (width < 9) {
			++width;
		}
	}

	if (width == 0) {
		error("Invalid graphics constant, no digits after '`'");
	} else if (width == 9) {
		warning(
		    WARNING_LARGE_CONSTANT, "Graphics constant is too large; only first 8 pixels considered"
		);
	}
	if (nonDigit) {
		error("Invalid graphics constant, trailing '_'");
	}

	return bitPlaneUpper << 8 | bitPlaneLower;
}

// Functions to read identifiers and keywords

static Token readIdentifier(char firstChar, bool raw) {
	std::string identifier(1, firstChar);
	int tokenType = firstChar == '.' ? T_(LOCAL) : T_(SYMBOL);

	// Continue reading while the char is in the identifier charset
	for (int c = peek(); continuesIdentifier(c); c = nextChar()) {
		identifier += c;

		// If the char was a dot, the identifier is a local label
		if (c == '.') {
			tokenType = T_(LOCAL);
		}
	}

	// Attempt to check for a keyword if the identifier is not raw or a local label
	if (!raw && tokenType != T_(LOCAL)) {
		if (auto search = keywordDict.find(identifier); search != keywordDict.end()) {
			return Token(search->second);
		}
	}

	// Label scopes `.` and `..` are the only nonlocal identifiers that start with a dot
	if (identifier.find_first_not_of('.') == identifier.npos) {
		tokenType = T_(SYMBOL);
	}

	return Token(tokenType, identifier);
}

// Functions to read strings

static std::pair<Symbol const *, std::shared_ptr<std::string>> readInterpolation(size_t depth) {
	if (depth > options.maxRecursionDepth) {
		fatal("Recursion limit (%zu) exceeded", options.maxRecursionDepth);
	}

	std::string fmtBuf;
	FormatSpec fmt{};

	for (;;) {
		// Use `consumeChar()` since `peek()` might expand nested interpolations and recursively
		// call `readInterpolation()`, which can cause stack overflow.
		if (consumeChar('{')) {
			if (auto interp = readInterpolation(depth + 1); interp.first && interp.second) {
				beginExpansion(interp.second, interp.first->name);
			}
			continue; // Restart, reading from the new buffer
		} else if (int c = peek(); c == EOF || isNewline(c) || c == '"') {
			error("Missing '}'");
			break;
		} else if (c == '}') {
			shiftChar();
			break;
		} else if (c == ':' && !fmt.isFinished()) { // Format spec, only once
			shiftChar();
			for (char f : fmtBuf) {
				fmt.useCharacter(f);
			}
			fmt.finishCharacters();
			if (!fmt.isValid()) {
				error("Invalid format spec \"%s\"", fmtBuf.c_str());
			}
			fmtBuf.clear(); // Now that format has been set, restart at beginning of string
		} else {
			shiftChar();
			fmtBuf += c;
		}
	}

	if (fmtBuf.starts_with('#')) {
		// Skip a '#' raw symbol prefix, but after expanding any nested interpolations.
		fmtBuf.erase(0, 1);
	} else if (keywordDict.find(fmtBuf) != keywordDict.end()) {
		// Don't allow symbols that alias keywords without a '#' prefix.
		error(
		    "Interpolated symbol `%s` is a reserved keyword; add a '#' prefix to use it as a raw "
		    "symbol",
		    fmtBuf.c_str()
		);
		return {nullptr, nullptr};
	}

	if (Symbol const *sym = sym_FindScopedValidSymbol(fmtBuf); !sym || !sym->isDefined()) {
		if (sym_IsPurgedScoped(fmtBuf)) {
			error("Interpolated symbol `%s` does not exist; it was purged", fmtBuf.c_str());
		} else {
			error("Interpolated symbol `%s` does not exist", fmtBuf.c_str());
		}
		return {sym, nullptr};
	} else if (sym->type == SYM_EQUS) {
		auto buf = std::make_shared<std::string>();
		fmt.appendString(*buf, *sym->getEqus());
		return {sym, buf};
	} else if (sym->isNumeric()) {
		auto buf = std::make_shared<std::string>();
		fmt.appendNumber(*buf, sym->getConstantValue());
		return {sym, buf};
	} else {
		error("Interpolated symbol `%s` is not a numeric or string symbol", fmtBuf.c_str());
		return {sym, nullptr};
	}
}

static void appendExpandedString(std::string &str, std::string const &expanded) {
	if (lexerState->mode != LEXER_RAW) {
		str.append(expanded);
		return;
	}

	for (char c : expanded) {
		// Escape characters that need escaping
		switch (c) {
		case '\n':
			str += "\\n";
			break;
			// LCOV_EXCL_START
		case '\r':
			// A literal CR in a string may get treated as a LF, so '\r' is not tested.
			str += "\\r";
			break;
			// LCOV_EXCL_STOP
		case '\t':
			str += "\\t";
			break;
		case '\0':
			str += "\\0";
			break;
		case '\\':
		case '"':
		case '\'':
		case '{':
			str += '\\';
			[[fallthrough]];
		default:
			str += c;
			break;
		}
	}
}

static void appendCharInLiteral(std::string &str, int c) {
	bool rawMode = lexerState->mode == LEXER_RAW;

	// Symbol interpolation
	if (c == '{') {
		// We'll be exiting the string/character scope, so re-enable expansions
		lexerState->disableExpansions = false;
		if (auto interp = readInterpolation(0); interp.second) {
			appendExpandedString(str, *interp.second);
		}
		lexerState->disableExpansions = true;
		return;
	}

	// Regular characters will just get copied
	if (c != '\\') {
		str += c;
		return;
	}

	c = peek();
	switch (c) {
	// Character escape
	case '\\':
	case '"':
	case '\'':
	case '{':
	case '}':
		if (rawMode) {
			str += '\\';
		}
		str += c;
		shiftChar();
		break;
	case 'n':
		str += rawMode ? "\\n" : "\n";
		shiftChar();
		break;
	case 'r':
		str += rawMode ? "\\r" : "\r";
		shiftChar();
		break;
	case 't':
		str += rawMode ? "\\t" : "\t";
		shiftChar();
		break;
	case '0':
		if (rawMode) {
			str += "\\0";
		} else {
			str += '\0';
		}
		shiftChar();
		break;

	// Line continuation
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		discardLineContinuation();
		break;

	// Macro arg
	case '@':
	case '#':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '<':
		if (std::shared_ptr<std::string> arg = readMacroArg(); arg) {
			appendExpandedString(str, *arg);
		}
		break;

	case EOF: // Can't really print that one
		error("Illegal character escape '\\' at end of input");
		str += '\\';
		break;

	default:
		error("Illegal character escape %s", printChar(c));
		str += c;
		shiftChar();
		break;
	}
}

static void readString(std::string &str, bool rawString) {
	Defer reenableExpansions = scopedDisableExpansions();

	bool rawMode = lexerState->mode == LEXER_RAW;

	// We reach this function after reading a single quote, but we also support triple quotes
	bool multiline = false;
	if (rawMode) {
		str += '"';
	}
	if (peek() == '"') {
		if (rawMode) {
			str += '"';
		}
		shiftChar();
		// Use `consumeChar()` since `peek()` would mark the third character here as "painted blue"
		// whether or not it is a third quote, which would incorrectly prevent expansions right
		// after an empty string "".
		if (!consumeChar('"')) {
			// "" is an empty string, skip the loop
			return;
		}
		// """ begins a multi-line string
		if (rawMode) {
			str += '"';
		}
		multiline = true;
	}

	for (;;) {
		int c = peek();

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && isNewline(c))) {
			error("Unterminated string");
			return;
		}

		// We'll be staying in the string, so we can safely consume the char
		shiftChar();

		// Handle '\r' or '\n' (in multiline strings only, already handled above otherwise)
		if (isNewline(c)) {
			handleCRLF(c);
			nextLine();
			str += '\n';
			continue;
		}

		if (c != '"') {
			// Append the character or handle special ones
			if (rawString) {
				str += c;
			} else {
				appendCharInLiteral(str, c);
			}
			continue;
		}

		// Close the string and return if it's terminated
		if (!multiline) {
			if (rawMode) {
				str += c;
			}
			return;
		}
		// Only """ ends a multi-line string
		if (peek() != '"') {
			str += c;
			continue;
		}
		if (nextChar() != '"') {
			str += "\"\"";
			continue;
		}
		shiftChar();
		if (rawMode) {
			str += "\"\"\"";
		}
		return;
	}
}

static void readCharacter(std::string &str) {
	// This is essentially a simplified `readString`
	Defer reenableExpansions = scopedDisableExpansions();

	bool rawMode = lexerState->mode == LEXER_RAW;

	// We reach this function after reading a single quote
	if (rawMode) {
		str += '\'';
	}

	for (;;) {
		switch (int c = peek(); c) {
		case '\r':
		case '\n':
		case EOF:
			// '\r', '\n' or EOF ends a character early
			error("Unterminated character");
			return;
		case '\'':
			// Close the character and return if it's terminated
			shiftChar();
			if (rawMode) {
				str += c;
			}
			return;
		default:
			// Append the character or handle special ones
			shiftChar();
			appendCharInLiteral(str, c);
		}
	}
}

// Lexer core

static Token yylex_SKIP_TO_ENDC(); // Forward declaration for `yylex_NORMAL`

// Must stay in sync with the `switch` in `yylex_NORMAL`!
static bool isGarbageCharacter(int c) {
	// Whitespace characters are not garbage, even the non-"printable" ones
	if (isWhitespace(c)) {
		return false;
	}
	// Printable characters which are nevertheless garbage: braces should have been interpolated
	if (c == '{' || c == '}') {
		return true;
	}
	// All other printable characters are not garbage (i.e. `yylex_NORMAL` handles them), and
	// all other nonprintable characters are garbage (including '\0' and EOF)
	return !isPrintable(c);
}

static void reportGarbageCharacters(int c) {
	// '#' can be garbage if it doesn't start a raw string or identifier
	assume(isGarbageCharacter(c) || c == '#');
	bool isAscii = isPrintable(c);
	if (isGarbageCharacter(peek())) {
		// At least two characters are garbage; group them into one error report
		std::string garbage = printChar(c);
		while (isGarbageCharacter(peek())) {
			c = bumpChar();
			isAscii &= isPrintable(c);
			garbage += ", ";
			garbage += printChar(c);
		}
		error("Invalid characters %s%s", garbage.c_str(), isAscii ? "" : " (is the file UTF-8?)");
	} else {
		error("Invalid character %s%s", printChar(c), isAscii ? "" : " (is the file UTF-8?)");
	}
}

static Token oneOrTwo(int c, int longer, int shorter) {
	if (peek() == c) {
		shiftChar();
		return Token(longer);
	}
	return Token(shorter);
}

static Token oneOrTwo(int c1, int longer1, int c2, int longer2, int shorter) {
	if (int c = peek(); c == c1) {
		shiftChar();
		return Token(longer1);
	} else if (c == c2) {
		shiftChar();
		return Token(longer2);
	} else {
		return Token(shorter);
	}
}

static Token yylex_NORMAL() {
	if (int nextToken = lexerState->nextToken; nextToken) {
		lexerState->nextToken = 0;
		return Token(nextToken);
	}

	for (;; lexerState->atLineStart = false) {
		int c = bumpChar();

		switch (c) {
			// Ignore blank space and comments

		case ';':
			discardComment();
			[[fallthrough]];

		case ' ':
		case '\t':
			continue;

			// Handle unambiguous single-char tokens

		case '~':
			return Token(T_(OP_NOT));

		case '?':
			return Token(T_(QUESTIONMARK));

		case '@': {
			std::string symName("@");
			return Token(T_(SYMBOL), symName);
		}

		case '(':
			return Token(T_(LPAREN));

		case ')':
			return Token(T_(RPAREN));

		case ',':
			return Token(T_(COMMA));

			// Handle ambiguous 1- or 2-char tokens

		case '[': // Either [ or [[
			return oneOrTwo('[', T_(LBRACKS), T_(LBRACK));

		case ']': // Either ] or ]]
			if (peek() == ']') {
				shiftChar();
				// `[[ Fragment literals ]]` inject an EOL token to end their contents
				// even without a newline. Retroactively lex the `]]` after it.
				lexerState->nextToken = T_(RBRACKS);
				return Token(T_(EOL));
			}
			return Token(T_(RBRACK));

		case '+': // Either +=, ADD, or CAT
			return oneOrTwo('=', T_(POP_ADDEQ), '+', T_(OP_CAT), T_(OP_ADD));

		case '-': // Either -= or SUB
			return oneOrTwo('=', T_(POP_SUBEQ), T_(OP_SUB));

		case '*': // Either *=, MUL, or EXP
			return oneOrTwo('=', T_(POP_MULEQ), '*', T_(OP_EXP), T_(OP_MUL));

		case '/': // Either /=, DIV, or a block comment
			if (peek() == '*') {
				shiftChar();
				discardBlockComment();
				continue;
			}
			return oneOrTwo('=', T_(POP_DIVEQ), T_(OP_DIV));

		case '|': // Either |=, binary OR, or logical OR
			return oneOrTwo('=', T_(POP_OREQ), '|', T_(OP_LOGICOR), T_(OP_OR));

		case '^': // Either ^= or XOR
			return oneOrTwo('=', T_(POP_XOREQ), T_(OP_XOR));

		case '=': // Either assignment or EQ
			return oneOrTwo('=', T_(OP_LOGICEQU), T_(POP_EQUAL));

		case '!': // Either a NEQ or negation
			return oneOrTwo('=', T_(OP_LOGICNE), T_(OP_LOGICNOT));

			// Handle ambiguous 1-, 2-, or 3-char tokens

		case '<': // Either <<=, LT, LTE, or left shift
			if (peek() == '<') {
				shiftChar();
				return oneOrTwo('=', T_(POP_SHLEQ), T_(OP_SHL));
			}
			return oneOrTwo('=', T_(OP_LOGICLE), T_(OP_LOGICLT));

		case '>': // Either >>=, GT, GTE, or either kind of right shift
			if (peek() == '>') {
				shiftChar();
				return oneOrTwo('=', T_(POP_SHREQ), '>', T_(OP_USHR), T_(OP_SHR));
			}
			return oneOrTwo('=', T_(OP_LOGICGE), T_(OP_LOGICGT));

		case ':': // Either :, ::, or an anonymous label ref
			c = peek();
			if (c == '+' || c == '-') {
				std::string symName = readAnonLabelRef(c);
				return Token(T_(ANON), symName);
			}
			return oneOrTwo(':', T_(DOUBLE_COLON), T_(COLON));

			// Handle numbers

		case '0': // Decimal, fixed-point, or base-prefix number
			switch (peek()) {
			case 'x':
			case 'X':
				shiftChar();
				return Token(T_(NUMBER), readHexNumber("\"0x\""));
			case 'o':
			case 'O':
				shiftChar();
				return Token(T_(NUMBER), readOctalNumber("\"0o\""));
			case 'b':
			case 'B':
				shiftChar();
				return Token(T_(NUMBER), readBinaryNumber("\"0b\""));
			}
			[[fallthrough]];

			// Decimal or fixed-point number

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			uint32_t n = readDecimalNumber(c);

			if (peek() == '.') {
				shiftChar();
				n = readFractionalPart(n);
			}
			return Token(T_(NUMBER), n);
		}

		case '&': // Either &=, binary AND, logical AND, or an octal constant
			c = peek();
			if (isOctDigit(c) || c == '_') {
				return Token(T_(NUMBER), readOctalNumber("'&'"));
			}
			return oneOrTwo('=', T_(POP_ANDEQ), '&', T_(OP_LOGICAND), T_(OP_AND));

		case '%': // Either %=, MOD, or a binary constant
			c = peek();
			if (c == '0' || c == '1' || c == options.binDigits[0] || c == options.binDigits[1]
			    || c == '_') {
				return Token(T_(NUMBER), readBinaryNumber("'%'"));
			}
			return oneOrTwo('=', T_(POP_MODEQ), T_(OP_MOD));

		case '$': // Hex constant
			return Token(T_(NUMBER), readHexNumber("'$'"));

		case '`': // Gfx constant
			return Token(T_(NUMBER), readGfxConstant());

			// Handle string and character literals

		case '"': {
			std::string str;
			readString(str, false);
			return Token(T_(STRING), str);
		}

		case '\'': {
			std::string chr;
			readCharacter(chr);
			return Token(T_(CHARACTER), chr);
		}

			// Handle newlines and EOF

		case '\r':
			handleCRLF(c);
			[[fallthrough]];
		case '\n':
			return Token(T_(NEWLINE));

		case EOF:
			return Token(T_(YYEOF));

			// Handle line continuations

		case '\\':
			// Macro args were handled by `peek`, and character escapes do not exist
			// outside of string literals, so this must be a line continuation.
			discardLineContinuation();
			continue;

			// Handle raw strings... or fall through if '#' is not followed by '"'

		case '#':
			if (peek() == '"') {
				shiftChar();
				std::string str;
				readString(str, true);
				return Token(T_(STRING), str);
			}
			[[fallthrough]];

			// Handle identifiers... or report garbage characters

		default:
			bool raw = c == '#';
			if (raw && startsIdentifier(peek())) {
				c = bumpChar();
			} else if (!startsIdentifier(c)) {
				reportGarbageCharacters(c);
				continue;
			}

			Token token = readIdentifier(c, raw);

			// An ELIF after a taken IF needs to not evaluate its condition
			if (token.type == T_(POP_ELIF) && lexerState->lastToken == T_(NEWLINE)
			    && lexer_GetIFDepth() > 0 && lexer_RanIFBlock() && !lexer_ReachedELSEBlock()) {
				return yylex_SKIP_TO_ENDC();
			}

			// If a keyword, don't try to expand
			if (token.type != T_(SYMBOL) && token.type != T_(LOCAL)) {
				return token;
			}

			// `token` is either a `SYMBOL` or a `LOCAL`, and both have a `std::string` value.
			assume(std::holds_alternative<std::string>(token.value));

			// Raw symbols and local symbols cannot be string expansions
			if (!raw && token.type == T_(SYMBOL) && lexerState->expandStrings) {
				// Attempt string expansion
				if (Symbol const *sym = sym_FindExactSymbol(std::get<std::string>(token.value));
				    sym && sym->type == SYM_EQUS) {
					beginExpansion(sym->getEqus(), sym->name);
					return yylex_NORMAL(); // Tail recursion
				}
			}

			// We need to distinguish between:
			// - label definitions (which are followed by a ':' and use the token `LABEL`)
			// - quiet macro invocations (which are followed by a '?' and use the token `QMACRO`)
			// - regular macro invocations (which use the token `SYMBOL`)
			//
			// If we had one `IDENTIFIER` token, the parser would need to perform "lookahead" to
			// determine which rule applies. But since macros need to enter "raw" mode to parse
			// their arguments, which may not even be valid tokens in "normal" mode, we cannot use
			// lookahead to check for the presence of a `COLON` or `QUESTIONMARK`.
			//
			// Instead, we have separate `SYMBOL`, `LABEL`, and `QMACRO` tokens, and decide which
			// one to lex depending on the character *immediately* following the identifier.
			// Thus "name:" is a label definition, and "name?" is a quiet macro invocation, but
			// "name :" and "name ?" and just "name" are all regular macro invocations.
			if (token.type == T_(SYMBOL)) {
				c = peek();
				token.type = c == ':' ? T_(LABEL) : c == '?' ? T_(QMACRO) : T_(SYMBOL);
			}

			return token;
		}
	}
}

static Token yylex_RAW() {
	// This is essentially a highly modified `readString`
	std::string str;
	size_t parenDepth = 0;
	int c;

	// Trim left spaces (stops at a block comment)
	for (;;) {
		c = peek();
		if (isBlankSpace(c)) {
			shiftChar();
		} else if (c == '\\') {
			c = nextChar();
			// If not a line continuation, handle as a normal char
			if (!isWhitespace(c)) {
				goto backslash;
			}
			// Line continuations count as "space"
			discardLineContinuation();
		} else {
			break;
		}
	}

	for (;;) {
		c = peek();

		switch (c) {
		case '"': // String literals inside macro args
			shiftChar();
			readString(str, false);
			break;

		case '\'': // Character literals inside macro args
			shiftChar();
			readCharacter(str);
			break;

		case '#': // Raw string literals inside macro args
			str += c;
			if (nextChar() == '"') {
				shiftChar();
				readString(str, true);
			}
			break;

		case ';': // Comments inside macro args
			discardComment();
			c = peek();
			[[fallthrough]];
		case '\r': // End of line
		case '\n':
		case EOF:
			goto finish;

		case '/': // Block comments inside macro args
			if (nextChar() == '*') {
				shiftChar();
				discardBlockComment();
				continue;
			}
			str += c; // Append the slash
			break;

		case ',': // End of macro arg
			if (parenDepth == 0) {
				goto finish;
			}
			goto append;

		case '(': // Open parentheses inside macro args
			if (parenDepth < UINT_MAX) {
				++parenDepth;
			}
			goto append;

		case ')': // Close parentheses inside macro args
			if (parenDepth > 0) {
				--parenDepth;
			}
			goto append;

		case '\\': // Character escape
			c = nextChar();

backslash:
			switch (c) {
			case ',': // Escapes only valid inside a macro arg
			case '(':
			case ')':
			case '\\': // Escapes shared with string literals
			case '"':
			case '\'':
			case '{':
			case '}':
				break;

			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case '0':
				c = '\0';
				break;

			case ' ':
			case '\t':
			case '\r':
			case '\n':
				discardLineContinuation();
				continue;

			case EOF: // Can't really print that one
				error("Illegal character escape '\\' at end of input");
				c = '\\';
				break;

				// Macro args were already handled by peek, so '\@',
				// '\#', and '\0'-'\9' should not occur here.

			default:
				error("Illegal character escape %s", printChar(c));
				break;
			}
			[[fallthrough]];

		default: // Regular characters will just get copied
append:
			str += c;
			shiftChar();
			break;
		}
	}

finish: // Can't `break` out of a nested `for`-`switch`
	// Trim right blank space
	auto rightPos = std::find_if_not(str.rbegin(), str.rend(), isBlankSpace);
	str.resize(rightPos.base() - str.begin());

	// Returning COMMAs to the parser would mean that two consecutive commas
	// (i.e. an empty argument) need to return two different tokens (STRING
	// then COMMA) without advancing the read. To avoid this, commas in raw
	// mode end the current macro argument but are not tokenized themselves.
	if (c == ',') {
		shiftChar();
		return Token(T_(STRING), str);
	}

	// The last argument may end in a trailing comma, newline, or EOF.
	// To allow trailing commas, raw mode will continue after the last
	// argument, immediately lexing the newline or EOF again (i.e. with
	// an empty raw string before it). This will not be treated as a
	// macro argument. To pass an empty last argument, use a second
	// trailing comma.
	if (!str.empty()) {
		return Token(T_(STRING), str);
	}

	lexer_SetMode(LEXER_NORMAL);

	if (isNewline(c)) {
		shiftChar();
		handleCRLF(c);
		return Token(T_(NEWLINE));
	}

	return Token(T_(YYEOF));
}

static int skipPastEOL() {
	if (lexerState->atLineStart) {
		lexerState->atLineStart = false;
		return skipChars(isBlankSpace);
	}

	for (;;) {
		if (int c = bumpChar(); c == EOF) {
			return EOF;
		} else if (isNewline(c)) {
			handleCRLF(c);
			nextLine();
			return skipChars(isBlankSpace);
		} else if (c == '\\') {
			// Unconditionally skip the next char, including line continuations
			c = bumpChar();
			if (isNewline(c)) {
				handleCRLF(c);
				nextLine();
			}
		}
	}
}

// This function uses the fact that `IF` and `REPT` constructs are only valid
// when there's nothing before them on their lines. This enables filtering
// "meaningful" tokens (at line start) vs. "meaningless" (everything else) ones.
// It's especially important due to macro args not being handled in this
// state, and lexing them in "normal" mode potentially producing such tokens.
static Token skipToLeadingIdentifier() {
	for (;;) {
		if (int c = skipPastEOL(); c == EOF) {
			return Token(T_(YYEOF));
		} else if (startsIdentifier(c)) {
			shiftChar();
			return readIdentifier(c, false);
		}
	}
}

static Token skipIfBlock(bool toEndc) {
	lexer_SetMode(LEXER_NORMAL);

	Defer reenableExpansions = scopedDisableExpansions();
	for (uint32_t startingDepth = lexer_GetIFDepth();;) {
		switch (Token token = skipToLeadingIdentifier(); token.type) {
		case T_(YYEOF):
			return token;

		case T_(POP_IF):
			lexer_IncIFDepth();
			break;

		case T_(POP_ELIF):
			if (lexer_ReachedELSEBlock()) {
				// This should be redundant, as the parser handles this error first.
				fatal("Found `ELIF` after an `ELSE` block"); // LCOV_EXCL_LINE
			}
			if (!toEndc && lexer_GetIFDepth() == startingDepth) {
				return token;
			}
			break;

		case T_(POP_ELSE):
			if (lexer_ReachedELSEBlock()) {
				fatal("Found `ELSE` after an `ELSE` block");
			}
			lexer_ReachELSEBlock();
			if (!toEndc && lexer_GetIFDepth() == startingDepth) {
				return token;
			}
			break;

		case T_(POP_ENDC):
			if (lexer_GetIFDepth() == startingDepth) {
				return token;
			}
			lexer_DecIFDepth();
			break;
		}
	}
}

static Token yylex_SKIP_TO_ELIF() {
	return skipIfBlock(false);
}

static Token yylex_SKIP_TO_ENDC() {
	return skipIfBlock(true);
}

static Token yylex_SKIP_TO_ENDR() {
	lexer_SetMode(LEXER_NORMAL);

	// This does not have to look for an `ENDR` token because the entire `REPT` or `FOR` body has
	// been captured into the current fstack context, so it can just skip to the end of that
	// context, which yields an EOF.
	Defer reenableExpansions = scopedDisableExpansions();
	for (;;) {
		switch (Token token = skipToLeadingIdentifier(); token.type) {
		case T_(YYEOF):
			return token;

		case T_(POP_IF):
			lexer_IncIFDepth();
			break;

		case T_(POP_ENDC):
			lexer_DecIFDepth();
			break;
		}
	}
}

yy::parser::symbol_type yylex() {
	if (lexerState->atLineStart && lexerStateEOL) {
		lexerState = lexerStateEOL;
		lexerStateEOL = nullptr;
	}
	if (lexerState->lastToken == T_(EOB) && yywrap()) {
		return yy::parser::make_YYEOF();
	}
	if (lexerState->atLineStart) {
		nextLine();
	}

	static Token (* const lexerModeFuncs[NB_LEXER_MODES])() = {
	    yylex_NORMAL,
	    yylex_RAW,
	    yylex_SKIP_TO_ELIF,
	    yylex_SKIP_TO_ENDC,
	    yylex_SKIP_TO_ENDR,
	};
	Token token = lexerModeFuncs[lexerState->mode]();

	// Captures end at their buffer's boundary no matter what
	if (token.type == T_(YYEOF) && !lexerState->capturing) {
		token.type = T_(EOB);
	}
	lexerState->lastToken = token.type;
	lexerState->atLineStart = token.type == T_(NEWLINE) || token.type == T_(EOB);

	// LCOV_EXCL_START
	verbosePrint(VERB_TRACE, "Lexed `%s` token\n", yy::parser::symbol_type(token.type).name());
	// LCOV_EXCL_STOP

	if (std::holds_alternative<uint32_t>(token.value)) {
		return yy::parser::symbol_type(token.type, std::get<uint32_t>(token.value));
	} else if (std::holds_alternative<std::string>(token.value)) {
		return yy::parser::symbol_type(token.type, std::get<std::string>(token.value));
	} else {
		assume(std::holds_alternative<std::monostate>(token.value));
		return yy::parser::symbol_type(token.type);
	}
}

template<typename F>
static Capture makeCapture(char const *name, F callback) {
	// Due to parser internals, it reads the EOL after the expression before calling this.
	// Thus, we don't need to keep one in the buffer afterwards.
	// The following assumption checks that.
	assume(lexerState->atLineStart);

	assume(!lexerState->capturing && lexerState->captureBuf == nullptr);
	lexerState->capturing = true;
	lexerState->captureSize = 0;

	Capture capture = {
	    .lineNo = lexer_GetLineNo(), .span = {.ptr = nullptr, .size = 0}
	};
	if (std::holds_alternative<ViewedContent>(lexerState->content)
	    && lexerState->expansions.empty()) {
		auto &view = std::get<ViewedContent>(lexerState->content);
		capture.span.ptr = view.makeSharedContentPtr();
	} else {
		assume(lexerState->captureBuf == nullptr);
		lexerState->captureBuf = std::make_shared<std::vector<char>>();
		// We'll retrieve the capture buffer when done capturing
		assume(capture.span.ptr == nullptr);
	}

	Defer reenableExpansions = scopedDisableExpansions();
	for (;;) {
		nextLine();

		if (int c = skipChars(isBlankSpace); startsIdentifier(c)) {
			shiftChar();
			int tokenType = readIdentifier(c, false).type;
			if (size_t endTokenLength = callback(tokenType); endTokenLength > 0) {
				if (!capture.span.ptr) {
					// Retrieve the capture buffer now that we're done capturing
					capture.span.ptr = lexerState->makeSharedCaptureBufPtr();
				}
				// Subtract the length of the ending token; we know we have read it exactly, not
				// e.g. an interpolation or EQUS expansion, since those are disabled.
				capture.span.size = lexerState->captureSize - endTokenLength;
				break;
			}
		}

		// Just consume characters until EOL or EOF
		if (int c = skipChars([](int d) { return d != EOF && !isNewline(d); }); c == EOF) {
			error("Unterminated %s", name);
			capture.span = {.ptr = nullptr, .size = lexerState->captureSize};
			break;
		} else {
			assume(isNewline(c));
			shiftChar();
			handleCRLF(c);
		}
	}

	lexerState->atLineStart = false; // The ending token or EOF puts us past the start of the line
	lexerState->capturing = false;
	lexerState->captureBuf = nullptr;
	return capture;
}

Capture lexer_CaptureRept() {
	size_t depth = 0;
	return makeCapture("loop (`REPT`/`FOR` block)", [&depth](int tokenType) {
		if (tokenType == T_(POP_REPT) || tokenType == T_(POP_FOR)) {
			++depth;
		} else if (tokenType == T_(POP_ENDR)) {
			if (depth == 0) {
				return literal_strlen("ENDR");
			}
			--depth;
		}
		return 0;
	});
}

Capture lexer_CaptureMacro() {
	return makeCapture("macro definition", [](int tokenType) {
		return tokenType == T_(POP_ENDM) ? literal_strlen("ENDM") : 0;
	});
}
