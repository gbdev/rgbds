/* SPDX-License-Identifier: MIT */

#include "asm/lexer.hpp"
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#ifndef _MSC_VER
	#include <unistd.h>
#endif

#include "helpers.hpp" // assume, QUOTEDSTRLEN
#include "util.hpp"

#include "asm/fixpoint.hpp"
#include "asm/format.hpp"
#include "asm/fstack.hpp"
#include "asm/macro.hpp"
#include "asm/main.hpp"
#include "asm/rpn.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"
// Include this last so it gets all type & constant definitions
#include "parser.hpp" // For token definitions, generated from parser.y

// Neither MSVC nor MinGW provide `mmap`
#if defined(_MSC_VER) || defined(__MINGW32__)
// clang-format off
	// (we need these `include`s in this order)
	#define WIN32_LEAN_AND_MEAN // include less from windows.h
	#include <windows.h>   // target architecture
	#include <fileapi.h>   // CreateFileA
	#include <winbase.h>   // CreateFileMappingA
	#include <memoryapi.h> // MapViewOfFile
	#include <handleapi.h> // CloseHandle
// clang-format on

static char *mapFile(int fd, std::string const &path, size_t) {
	void *mappingAddr = nullptr;
	if (HANDLE file = CreateFileA(
	        path.c_str(),
	        GENERIC_READ,
	        FILE_SHARE_READ,
	        nullptr,
	        OPEN_EXISTING,
	        FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_RANDOM_ACCESS,
	        nullptr
	    );
	    file != INVALID_HANDLE_VALUE) {
		if (HANDLE mappingObj = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
		    mappingObj != INVALID_HANDLE_VALUE) {
			mappingAddr = MapViewOfFile(mappingObj, FILE_MAP_READ, 0, 0, 0);
			CloseHandle(mappingObj);
		}
		CloseHandle(file);
	}
	return (char *)mappingAddr;
}

struct FileUnmapDeleter {
	FileUnmapDeleter(size_t) {}

	void operator()(char *mappingAddr) { UnmapViewOfFile(mappingAddr); }
};

#else // defined(_MSC_VER) || defined(__MINGW32__)
	#include <sys/mman.h>

static char *mapFile(int fd, std::string const &path, size_t size) {
	void *mappingAddr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mappingAddr == MAP_FAILED && errno == ENOTSUP) {
		// The implementation may not support MAP_PRIVATE; try again with MAP_SHARED
		// instead, offering, I believe, weaker guarantees about external modifications to
		// the file while reading it. That's still better than not opening it at all, though.
		if (verbose)
			printf("mmap(%s, MAP_PRIVATE) failed, retrying with MAP_SHARED\n", path.c_str());
		mappingAddr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	}
	return mappingAddr != MAP_FAILED ? (char *)mappingAddr : nullptr;
}

struct FileUnmapDeleter {
	size_t mappingSize;

	FileUnmapDeleter(size_t mappingSize_) : mappingSize(mappingSize_) {}

	void operator()(char *mappingAddr) { munmap(mappingAddr, mappingSize); }
};

#endif // !( defined(_MSC_VER) || defined(__MINGW32__) )

using namespace std::literals;

// Bison 3.6 changed token "types" to "kinds"; cast to int for simple compatibility
#define T_(name) (int)yy::parser::token::name

struct Token {
	int type;
	std::variant<std::monostate, uint32_t, std::string> value;

	Token() : type(T_(NUMBER)), value(std::monostate{}) {}
	Token(int type_) : type(type_), value(std::monostate{}) {}
	Token(int type_, uint32_t value_) : type(type_), value(value_) {}
	Token(int type_, std::string const &value_) : type(type_), value(value_) {}
	Token(int type_, std::string &&value_) : type(type_), value(value_) {}
};

struct CaseInsensitive {
	// FNV-1a hash of an uppercased string
	size_t operator()(std::string const &str) const {
		size_t hash = 0x811C9DC5;

		for (char const &c : str)
			hash = (hash ^ toupper(c)) * 16777619;
		return hash;
	}

	// Compare two strings without case-sensitivity (by converting to uppercase)
	bool operator()(std::string const &str1, std::string const &str2) const {
		return std::equal(RANGE(str1), RANGE(str2), [](char c1, char c2) {
			return toupper(c1) == toupper(c2);
		});
	}
};

// Identifiers that are also keywords are listed here. This ONLY applies to ones
// that would normally be matched as identifiers! Check out `yylex_NORMAL` to
// see how this is used.
// Tokens / keywords not handled here are handled in `yylex_NORMAL`'s switch.
// This assumes that no two keywords have the same name.
static std::unordered_map<std::string, int, CaseInsensitive, CaseInsensitive> keywordDict = {
    {"ADC",           T_(Z80_ADC)          },
    {"ADD",           T_(Z80_ADD)          },
    {"AND",           T_(Z80_AND)          },
    {"BIT",           T_(Z80_BIT)          },
    {"CALL",          T_(Z80_CALL)         },
    {"CCF",           T_(Z80_CCF)          },
    {"CPL",           T_(Z80_CPL)          },
    {"CP",            T_(Z80_CP)           },
    {"DAA",           T_(Z80_DAA)          },
    {"DEC",           T_(Z80_DEC)          },
    {"DI",            T_(Z80_DI)           },
    {"EI",            T_(Z80_EI)           },
    {"HALT",          T_(Z80_HALT)         },
    {"INC",           T_(Z80_INC)          },
    {"JP",            T_(Z80_JP)           },
    {"JR",            T_(Z80_JR)           },
    {"LD",            T_(Z80_LD)           },
    {"LDI",           T_(Z80_LDI)          },
    {"LDD",           T_(Z80_LDD)          },
    {"LDIO",          T_(Z80_LDH)          },
    {"LDH",           T_(Z80_LDH)          },
    {"NOP",           T_(Z80_NOP)          },
    {"OR",            T_(Z80_OR)           },
    {"POP",           T_(Z80_POP)          },
    {"PUSH",          T_(Z80_PUSH)         },
    {"RES",           T_(Z80_RES)          },
    {"RETI",          T_(Z80_RETI)         },
    {"RET",           T_(Z80_RET)          },
    {"RLCA",          T_(Z80_RLCA)         },
    {"RLC",           T_(Z80_RLC)          },
    {"RLA",           T_(Z80_RLA)          },
    {"RL",            T_(Z80_RL)           },
    {"RRC",           T_(Z80_RRC)          },
    {"RRCA",          T_(Z80_RRCA)         },
    {"RRA",           T_(Z80_RRA)          },
    {"RR",            T_(Z80_RR)           },
    {"RST",           T_(Z80_RST)          },
    {"SBC",           T_(Z80_SBC)          },
    {"SCF",           T_(Z80_SCF)          },
    {"SET",           T_(Z80_SET)          },
    {"SLA",           T_(Z80_SLA)          },
    {"SRA",           T_(Z80_SRA)          },
    {"SRL",           T_(Z80_SRL)          },
    {"STOP",          T_(Z80_STOP)         },
    {"SUB",           T_(Z80_SUB)          },
    {"SWAP",          T_(Z80_SWAP)         },
    {"XOR",           T_(Z80_XOR)          },

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
    {"ALIGN",         T_(OP_ALIGN)         },

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

    {"STRCMP",        T_(OP_STRCMP)        },
    {"STRIN",         T_(OP_STRIN)         },
    {"STRRIN",        T_(OP_STRRIN)        },
    {"STRSUB",        T_(OP_STRSUB)        },
    {"STRLEN",        T_(OP_STRLEN)        },
    {"STRCAT",        T_(OP_STRCAT)        },
    {"STRUPR",        T_(OP_STRUPR)        },
    {"STRLWR",        T_(OP_STRLWR)        },
    {"STRRPL",        T_(OP_STRRPL)        },
    {"STRFMT",        T_(OP_STRFMT)        },

    {"CHARLEN",       T_(OP_CHARLEN)       },
    {"CHARSUB",       T_(OP_CHARSUB)       },
    {"INCHARMAP",     T_(OP_INCHARMAP)     },

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
 // There is no `T_(POP_RL)`; it's handled before as `T_(Z80_RL)`

    {"EQU",           T_(POP_EQU)          },
    {"EQUS",          T_(POP_EQUS)         },
    {"REDEF",         T_(POP_REDEF)        },

    {"PUSHS",         T_(POP_PUSHS)        },
    {"POPS",          T_(POP_POPS)         },
    {"PUSHO",         T_(POP_PUSHO)        },
    {"POPO",          T_(POP_POPO)         },

    {"OPT",           T_(POP_OPT)          },

    {".",             T_(PERIOD)           },
};

static bool isWhitespace(int c) {
	return c == ' ' || c == '\t';
}

static LexerState *lexerState = nullptr;
static LexerState *lexerStateEOL = nullptr;

void LexerState::clear(uint32_t lineNo_) {
	mode = LEXER_NORMAL;
	atLineStart = true; // yylex() will init colNo due to this
	lastToken = T_(YYEOF);

	ifStack.clear();

	capturing = false;
	captureBuf = nullptr;

	disableMacroArgs = false;
	disableInterpolation = false;
	macroArgScanDistance = 0;
	expandStrings = true;

	expansions.clear();

	lineNo = lineNo_; // Will be incremented at next line start
}

static void nextLine() {
	lexerState->lineNo++;
	lexerState->colNo = 1;
}

uint32_t lexer_GetIFDepth() {
	return lexerState->ifStack.size();
}

void lexer_IncIFDepth() {
	lexerState->ifStack.push_front({.ranIfBlock = false, .reachedElseBlock = false});
}

void lexer_DecIFDepth() {
	if (lexerState->ifStack.empty())
		fatalerror("Found ENDC outside an IF construct\n");

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

bool LexerState::setFileAsNextState(std::string const &filePath, bool updateStateNow) {
	if (filePath == "-") {
		path = "<stdin>";
		content.emplace<BufferedContent>(STDIN_FILENO);
		if (verbose)
			printf("Opening stdin\n");
	} else {
		struct stat statBuf;
		if (stat(filePath.c_str(), &statBuf) != 0) {
			error("Failed to stat file \"%s\": %s\n", filePath.c_str(), strerror(errno));
			return false;
		}
		path = filePath;

		int fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			error("Failed to open file \"%s\": %s\n", path.c_str(), strerror(errno));
			return false;
		}

		bool isMmapped = false;

		if (size_t size = (size_t)statBuf.st_size; statBuf.st_size > 0) {
			// Try using `mmap` for better performance
			if (char *mappingAddr = mapFile(fd, path, size); mappingAddr != nullptr) {
				close(fd);
				content.emplace<ViewedContent>(
				    std::shared_ptr<char[]>(mappingAddr, FileUnmapDeleter(size)), size
				);
				if (verbose)
					printf("File \"%s\" is mmap()ped\n", path.c_str());
				isMmapped = true;
			}
		}

		if (!isMmapped) {
			// Sometimes mmap() fails or isn't available, so have a fallback
			content.emplace<BufferedContent>(fd);
			if (verbose) {
				if (statBuf.st_size == 0) {
					printf("File \"%s\" is empty\n", path.c_str());
				} else {
					printf(
					    "File \"%s\" is opened; errno reports: %s\n", path.c_str(), strerror(errno)
					);
				}
			}
		}
	}

	clear(0);
	if (updateStateNow)
		lexerState = this;
	else
		lexerStateEOL = this;
	return true;
}

void LexerState::setViewAsNextState(char const *name, ContentSpan const &span, uint32_t lineNo_) {
	path = name; // Used to report read errors in `.peek()`
	content.emplace<ViewedContent>(span);
	clear(lineNo_);
	lexerStateEOL = this;
}

void lexer_RestartRept(uint32_t lineNo) {
	if (auto *view = std::get_if<ViewedContent>(&lexerState->content); view) {
		view->offset = 0;
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
	offset++;
	return offset > size();
}

BufferedContent::~BufferedContent() {
	close(fd);
}

void BufferedContent::advance() {
	assume(offset < LEXER_BUF_SIZE);
	offset++;
	if (offset == LEXER_BUF_SIZE)
		offset = 0; // Wrap around if necessary
	assume(size > 0);
	size--;
}

void BufferedContent::refill() {
	size_t target = LEXER_BUF_SIZE - size; // Aim: making the buf full

	// Compute the index we'll start writing to
	size_t startIndex = (offset + size) % LEXER_BUF_SIZE;

	// If the range to fill passes over the buffer wrapping point, we need two reads
	if (startIndex + target > LEXER_BUF_SIZE) {
		size_t nbExpectedChars = LEXER_BUF_SIZE - startIndex;
		size_t nbReadChars = readMore(startIndex, nbExpectedChars);

		startIndex += nbReadChars;
		if (startIndex == LEXER_BUF_SIZE)
			startIndex = 0;

		// If the read was incomplete, don't perform a second read
		target -= nbReadChars;
		if (nbReadChars < nbExpectedChars)
			target = 0;
	}
	if (target != 0)
		readMore(startIndex, target);
}

size_t BufferedContent::readMore(size_t startIndex, size_t nbChars) {
	// This buffer overflow made me lose WEEKS of my life. Never again.
	assume(startIndex + nbChars <= LEXER_BUF_SIZE);
	ssize_t nbReadChars = read(fd, &buf[startIndex], nbChars);

	if (nbReadChars == -1)
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path.c_str(), strerror(errno));

	size += nbReadChars;

	// `nbReadChars` cannot be negative, so it's fine to cast to `size_t`
	return (size_t)nbReadChars;
}

void lexer_SetMode(LexerMode mode) {
	lexerState->mode = mode;
}

void lexer_ToggleStringExpansion(bool enable) {
	lexerState->expandStrings = enable;
}

// Functions for the actual lexer to obtain characters

static void beginExpansion(std::shared_ptr<std::string> str, std::optional<std::string> name) {
	if (name)
		lexer_CheckRecursionDepth();

	// Do not expand empty strings
	if (str->empty())
		return;

	lexerState->expansions.push_front({.name = name, .contents = str, .offset = 0});
}

void lexer_CheckRecursionDepth() {
	if (lexerState->expansions.size() > maxRecursionDepth + 1)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);
}

static bool isMacroChar(char c) {
	return c == '@' || c == '#' || c == '<' || (c >= '1' && c <= '9');
}

// forward declarations for readBracketedMacroArgNum
static int peek();
static void shiftChar();
static uint32_t readNumber(int radix, uint32_t baseValue);
static bool startsIdentifier(int c);
static bool continuesIdentifier(int c);

static uint32_t readBracketedMacroArgNum() {
	bool disableMacroArgs = lexerState->disableMacroArgs;
	bool disableInterpolation = lexerState->disableInterpolation;
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	Defer restoreExpansions{[&] {
		lexerState->disableMacroArgs = disableMacroArgs;
		lexerState->disableInterpolation = disableInterpolation;
	}};

	uint32_t num = 0;
	int c = peek();
	bool empty = false;
	bool symbolError = false;

	if (c >= '0' && c <= '9') {
		num = readNumber(10, 0);
	} else if (startsIdentifier(c)) {
		std::string symName;

		for (; continuesIdentifier(c); c = peek()) {
			symName += c;
			shiftChar();
		}

		Symbol const *sym = sym_FindScopedValidSymbol(symName);

		if (!sym) {
			if (sym_IsPurgedScoped(symName))
				error("Bracketed symbol \"%s\" does not exist; it was purged\n", symName.c_str());
			else
				error("Bracketed symbol \"%s\" does not exist\n", symName.c_str());
			num = 0;
			symbolError = true;
		} else if (!sym->isNumeric()) {
			error("Bracketed symbol \"%s\" is not numeric\n", symName.c_str());
			num = 0;
			symbolError = true;
		} else {
			num = sym->getConstantValue();
		}
	} else {
		empty = true;
	}

	c = peek();
	shiftChar();
	if (c != '>') {
		error("Invalid character in bracketed macro argument %s\n", printChar(c));
		return 0;
	} else if (empty) {
		error("Empty bracketed macro argument\n");
		return 0;
	} else if (num == 0 && !symbolError) {
		error("Invalid bracketed macro argument '\\<0>'\n");
		return 0;
	} else {
		return num;
	}
}

static std::shared_ptr<std::string> readMacroArg(char name) {
	if (name == '@') {
		auto str = fstk_GetUniqueIDStr();
		if (!str) {
			error("'\\@' cannot be used outside of a macro or REPT/FOR block\n");
		}
		return str;
	} else if (name == '#') {
		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		auto str = macroArgs ? macroArgs->getAllArgs() : nullptr;
		if (!str) {
			error("'\\#' cannot be used outside of a macro\n");
		}
		return str;
	} else if (name == '<') {
		uint32_t num = readBracketedMacroArgNum();
		if (num == 0) {
			// The error was already reported by `readBracketedMacroArgNum`.
			return nullptr;
		}

		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		if (!macroArgs) {
			error("'\\<%" PRIu32 ">' cannot be used outside of a macro\n", num);
			return nullptr;
		}

		auto str = macroArgs->getArg(num);
		if (!str) {
			error("Macro argument '\\<%" PRIu32 ">' not defined\n", num);
		}
		return str;
	} else if (name == '0') {
		error("Invalid macro argument '\\0'\n");
		return nullptr;
	} else {
		assume(name >= '1' && name <= '9');

		MacroArgs *macroArgs = fstk_GetCurrentMacroArgs();
		if (!macroArgs) {
			error("'\\%c' cannot be used outside of a macro\n", name);
			return nullptr;
		}

		auto str = macroArgs->getArg(name - '0');
		if (!str) {
			error("Macro argument '\\%c' not defined\n", name);
		}
		return str;
	}
}

int LexerState::peekChar() {
	// This is `.peekCharAhead()` modified for zero lookahead distance
	for (Expansion &exp : expansions) {
		if (exp.offset < exp.size())
			return (uint8_t)(*exp.contents)[exp.offset];
	}

	if (auto *view = std::get_if<ViewedContent>(&content); view) {
		if (view->offset < view->span.size)
			return (uint8_t)view->span.ptr[view->offset];
	} else {
		assume(std::holds_alternative<BufferedContent>(content));
		auto &cbuf = std::get<BufferedContent>(content);
		if (cbuf.size == 0)
			cbuf.refill();
		assume(cbuf.offset < LEXER_BUF_SIZE);
		if (cbuf.size > 0)
			return (uint8_t)cbuf.buf[cbuf.offset];
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
		if (exp.offset + distance < exp.size())
			return (uint8_t)(*exp.contents)[exp.offset + distance];
		distance -= exp.size() - exp.offset;
	}

	if (auto *view = std::get_if<ViewedContent>(&content); view) {
		if (view->offset + distance < view->span.size)
			return (uint8_t)view->span.ptr[view->offset + distance];
	} else {
		assume(std::holds_alternative<BufferedContent>(content));
		auto &cbuf = std::get<BufferedContent>(content);
		assume(distance < LEXER_BUF_SIZE);
		if (cbuf.size <= distance)
			cbuf.refill();
		if (cbuf.size > distance)
			return (uint8_t)cbuf.buf[(cbuf.offset + distance) % LEXER_BUF_SIZE];
	}

	// If there aren't enough chars, give up
	return EOF;
}

// forward declarations for peek
static void shiftChar();
static std::shared_ptr<std::string> readInterpolation(size_t depth);

static int peek() {
	int c = lexerState->peekChar();

	if (lexerState->macroArgScanDistance > 0)
		return c;

	lexerState->macroArgScanDistance++; // Do not consider again

	if (c == '\\' && !lexerState->disableMacroArgs) {
		// If character is a backslash, check for a macro arg
		lexerState->macroArgScanDistance++;
		c = lexerState->peekCharAhead();
		if (isMacroChar(c)) {
			shiftChar();
			shiftChar();

			std::shared_ptr<std::string> str = readMacroArg(c);
			// If the macro arg is invalid or an empty string, it cannot be expanded,
			// so skip it and keep peeking.
			if (!str || str->empty()) {
				return peek();
			}

			beginExpansion(str, std::nullopt);

			// Assuming macro args can't be recursive (I'll be damned if a way
			// is found...), then we mark the entire macro arg as scanned.
			lexerState->macroArgScanDistance += str->length();

			c = str->front();
		} else {
			c = '\\';
		}
	} else if (c == '{' && !lexerState->disableInterpolation) {
		// If character is an open brace, do symbol interpolation
		shiftChar();

		if (auto str = readInterpolation(0); str) {
			beginExpansion(str, *str);
		}

		return peek();
	}

	return c;
}

static void shiftChar() {
	if (lexerState->capturing) {
		if (lexerState->captureBuf)
			lexerState->captureBuf->push_back(peek());
		lexerState->captureSize++;
	}

	lexerState->macroArgScanDistance--;

restart:
	if (!lexerState->expansions.empty()) {
		// Advance within the current expansion
		if (Expansion &exp = lexerState->expansions.front(); exp.advance()) {
			// When advancing would go past an expansion's end,
			// move up to its parent and try again to advance
			lexerState->expansions.pop_front();
			goto restart;
		}
	} else {
		// Advance within the file contents
		lexerState->colNo++;
		if (auto *view = std::get_if<ViewedContent>(&lexerState->content); view) {
			view->offset++;
		} else {
			assume(std::holds_alternative<BufferedContent>(lexerState->content));
			auto &cbuf = std::get<BufferedContent>(lexerState->content);
			cbuf.advance();
		}
	}
}

static int nextChar() {
	int c = peek();

	// If not at EOF, advance read position
	if (c != EOF)
		shiftChar();
	return c;
}

static void handleCRLF(int c) {
	if (c == '\r' && peek() == '\n')
		shiftChar();
}

static auto scopedDisableExpansions() {
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	return Defer{[&] {
		lexerState->disableMacroArgs = false;
		lexerState->disableInterpolation = false;
	}};
}

// "Services" provided by the lexer to the rest of the program

uint32_t lexer_GetLineNo() {
	return lexerState->lineNo;
}

uint32_t lexer_GetColNo() {
	return lexerState->colNo;
}

void lexer_DumpStringExpansions() {
	if (!lexerState)
		return;

	for (Expansion &exp : lexerState->expansions) {
		// Only register EQUS expansions, not string args
		if (exp.name)
			fprintf(stderr, "while expanding symbol \"%s\"\n", exp.name->c_str());
	}
}

// Functions to discard non-tokenized characters

static void discardBlockComment() {
	Defer reenableExpansions = scopedDisableExpansions();
	for (;;) {
		int c = nextChar();

		switch (c) {
		case EOF:
			error("Unterminated block comment\n");
			return;
		case '\r':
			// Handle CRLF before nextLine() since shiftChar updates colNo
			handleCRLF(c);
			[[fallthrough]];
		case '\n':
			if (lexerState->expansions.empty())
				nextLine();
			continue;
		case '/':
			if (peek() == '*') {
				warning(WARNING_NESTED_COMMENT, "/* in block comment\n");
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
	for (;; shiftChar()) {
		int c = peek();

		if (c == EOF || c == '\r' || c == '\n')
			break;
	}
}

static void discardLineContinuation() {
	for (;;) {
		int c = peek();

		if (isWhitespace(c)) {
			shiftChar();
		} else if (c == '\r' || c == '\n') {
			shiftChar();
			// Handle CRLF before nextLine() since shiftChar updates colNo
			handleCRLF(c);
			if (lexerState->expansions.empty())
				nextLine();
			break;
		} else if (c == ';') {
			discardComment();
		} else {
			error("Begun line continuation, but encountered character %s\n", printChar(c));
			break;
		}
	}
}

// Functions to read tokenizable values

static std::string readAnonLabelRef(char c) {
	uint32_t n = 0;

	// We come here having already peeked at one char, so no need to do it again
	do {
		shiftChar();
		n++;
	} while (peek() == c);

	return sym_MakeAnonLabelName(n, c == '-');
}

static uint32_t readNumber(int radix, uint32_t baseValue) {
	uint32_t value = baseValue;

	for (;; shiftChar()) {
		int c = peek();

		if (c == '_')
			continue;
		else if (c < '0' || c > '0' + radix - 1)
			break;
		if (value > (UINT32_MAX - (c - '0')) / radix)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * radix + (c - '0');
	}

	return value;
}

static uint32_t readFractionalPart(uint32_t integer) {
	uint32_t value = 0, divisor = 1;
	uint8_t precision = 0;
	enum {
		READFRACTIONALPART_DIGITS,
		READFRACTIONALPART_PRECISION,
		READFRACTIONALPART_PRECISION_DIGITS,
	} state = READFRACTIONALPART_DIGITS;

	for (;; shiftChar()) {
		int c = peek();

		if (state == READFRACTIONALPART_DIGITS) {
			if (c == '_') {
				continue;
			} else if (c == 'q' || c == 'Q') {
				state = READFRACTIONALPART_PRECISION;
				continue;
			} else if (c < '0' || c > '9') {
				break;
			}
			if (divisor > (UINT32_MAX - (c - '0')) / 10) {
				warning(WARNING_LARGE_CONSTANT, "Precision of fixed-point constant is too large\n");
				// Discard any additional digits
				shiftChar();
				while (c = peek(), (c >= '0' && c <= '9') || c == '_')
					shiftChar();
				break;
			}
			value = value * 10 + (c - '0');
			divisor *= 10;
		} else {
			if (c == '.' && state == READFRACTIONALPART_PRECISION) {
				state = READFRACTIONALPART_PRECISION_DIGITS;
				continue;
			} else if (c < '0' || c > '9') {
				break;
			}
			precision = precision * 10 + (c - '0');
		}
	}

	if (precision == 0) {
		if (state >= READFRACTIONALPART_PRECISION)
			error("Invalid fixed-point constant, no significant digits after 'q'\n");
		precision = fixPrecision;
	} else if (precision > 31) {
		error("Fixed-point constant precision must be between 1 and 31\n");
		precision = fixPrecision;
	}

	if (integer >= ((uint64_t)1 << (32 - precision)))
		warning(WARNING_LARGE_CONSTANT, "Magnitude of fixed-point constant is too large\n");

	// Cast to unsigned avoids undefined overflow behavior
	uint32_t fractional = (uint32_t)round((double)value / divisor * pow(2.0, precision));

	return (integer << precision) | fractional;
}

char binDigits[2];

static uint32_t readBinaryNumber() {
	uint32_t value = 0;

	for (;; shiftChar()) {
		int c = peek();
		int bit;

		// Check for '_' after digits in case one of the digits is '_'
		if (c == binDigits[0])
			bit = 0;
		else if (c == binDigits[1])
			bit = 1;
		else if (c == '_')
			continue;
		else
			break;
		if (value > (UINT32_MAX - bit) / 2)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * 2 + bit;
	}

	return value;
}

static uint32_t readHexNumber() {
	uint32_t value = 0;
	bool empty = true;

	for (;; shiftChar()) {
		int c = peek();

		if (c >= 'a' && c <= 'f')
			c = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			c = c - 'A' + 10;
		else if (c >= '0' && c <= '9')
			c = c - '0';
		else if (c == '_' && !empty)
			continue;
		else
			break;

		if (value > (UINT32_MAX - c) / 16)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * 16 + c;

		empty = false;
	}

	if (empty)
		error("Invalid integer constant, no digits after '$'\n");

	return value;
}

char gfxDigits[4];

static uint32_t readGfxConstant() {
	uint32_t bitPlaneLower = 0, bitPlaneUpper = 0;
	uint8_t width = 0;

	for (;; shiftChar()) {
		int c = peek();
		uint32_t pixel;

		// Check for '_' after digits in case one of the digits is '_'
		if (c == gfxDigits[0])
			pixel = 0;
		else if (c == gfxDigits[1])
			pixel = 1;
		else if (c == gfxDigits[2])
			pixel = 2;
		else if (c == gfxDigits[3])
			pixel = 3;
		else if (c == '_' && width > 0)
			continue;
		else
			break;

		if (width < 8) {
			bitPlaneLower = bitPlaneLower << 1 | (pixel & 1);
			bitPlaneUpper = bitPlaneUpper << 1 | (pixel >> 1);
		}
		if (width < 9)
			width++;
	}

	if (width == 0)
		error("Invalid graphics constant, no digits after '`'\n");
	else if (width == 9)
		warning(
		    WARNING_LARGE_CONSTANT,
		    "Graphics constant is too long, only first 8 pixels considered\n"
		);

	return bitPlaneUpper << 8 | bitPlaneLower;
}

// Functions to read identifiers & keywords

static bool startsIdentifier(int c) {
	// Anonymous labels internally start with '!'
	return (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || c == '.' || c == '_';
}

static bool continuesIdentifier(int c) {
	return startsIdentifier(c) || (c <= '9' && c >= '0') || c == '#' || c == '@';
}

static Token readIdentifier(char firstChar) {
	// Lex while checking for a keyword
	std::string identifier(1, firstChar);
	int tokenType = firstChar == '.' ? T_(LOCAL_ID) : T_(ID);

	// Continue reading while the char is in the symbol charset
	for (int c = peek(); continuesIdentifier(c); c = peek()) {
		shiftChar();

		// Write the char to the identifier's name
		identifier += c;

		// If the char was a dot, mark the identifier as local
		if (c == '.')
			tokenType = T_(LOCAL_ID);
	}

	// Attempt to check for a keyword
	auto search = keywordDict.find(identifier.c_str());
	return search != keywordDict.end() ? Token(search->second) : Token(tokenType, identifier);
}

// Functions to read strings

static std::shared_ptr<std::string> readInterpolation(size_t depth) {
	if (depth > maxRecursionDepth)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);

	std::string fmtBuf;
	FormatSpec fmt{};
	bool disableInterpolation = lexerState->disableInterpolation;

	// In a context where `lexerState->disableInterpolation` is true, `peek` will expand
	// nested interpolations itself, which can lead to stack overflow. This lets
	// `readInterpolation` handle its own nested expansions, increasing `depth` each time.
	lexerState->disableInterpolation = true;

	for (;;) {
		int c = peek();

		if (c == '{') { // Nested interpolation
			shiftChar();
			auto str = readInterpolation(depth + 1);

			beginExpansion(str, *str);
			continue; // Restart, reading from the new buffer
		} else if (c == EOF || c == '\r' || c == '\n' || c == '"') {
			error("Missing }\n");
			break;
		} else if (c == '}') {
			shiftChar();
			break;
		} else if (c == ':' && !fmt.isFinished()) { // Format spec, only once
			shiftChar();
			for (char f : fmtBuf)
				fmt.useCharacter(f);
			fmt.finishCharacters();
			if (!fmt.isValid())
				error("Invalid format spec '%s'\n", fmtBuf.c_str());
			fmtBuf.clear(); // Now that format has been set, restart at beginning of string
		} else {
			shiftChar();
			fmtBuf += c;
		}
	}

	// Don't return before `lexerState->disableInterpolation` is reset!
	lexerState->disableInterpolation = disableInterpolation;

	Symbol const *sym = sym_FindScopedValidSymbol(fmtBuf);

	if (!sym || !sym->isDefined()) {
		if (sym_IsPurgedScoped(fmtBuf))
			error("Interpolated symbol \"%s\" does not exist; it was purged\n", fmtBuf.c_str());
		else
			error("Interpolated symbol \"%s\" does not exist\n", fmtBuf.c_str());
	} else if (sym->type == SYM_EQUS) {
		auto buf = std::make_shared<std::string>();
		fmt.appendString(*buf, *sym->getEqus());
		return buf;
	} else if (sym->isNumeric()) {
		auto buf = std::make_shared<std::string>();
		fmt.appendNumber(*buf, sym->getConstantValue());
		return buf;
	} else {
		error("Interpolated symbol \"%s\" is not a numeric or string symbol\n", fmtBuf.c_str());
	}
	return nullptr;
}

static void appendEscapedString(std::string &str, std::string const &escape) {
	for (char c : escape) {
		// Escape characters that need escaping
		switch (c) {
		case '\\':
		case '"':
		case '{':
			str += '\\';
			[[fallthrough]];
		default:
			str += c;
			break;
		case '\n':
			str += "\\n";
			break;
		case '\r':
			str += "\\r";
			break;
		case '\t':
			str += "\\t";
			break;
		case '\0':
			str += "\\0";
			break;
		}
	}
}

static std::string readString(bool raw) {
	Defer reenableExpansions = scopedDisableExpansions();

	// We reach this function after reading a single quote, but we also support triple quotes
	bool multiline = false;
	if (peek() == '"') {
		shiftChar();
		if (peek() == '"') {
			// """ begins a multi-line string
			shiftChar();
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			return ""s;
		}
	}

	for (std::string str = ""s;;) {
		int c = peek();

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			return str;
		}

		// We'll be staying in the string, so we can safely consume the char
		shiftChar();

		// Handle '\r' or '\n' (in multiline strings only, already handled above otherwise)
		if (c == '\r' || c == '\n') {
			// Handle CRLF before nextLine() since shiftChar updates colNo
			handleCRLF(c);
			nextLine();
			c = '\n';
		}

		switch (c) {
		case '"':
			if (multiline) {
				// Only """ ends a multi-line string
				if (peek() != '"')
					break;
				shiftChar();
				if (peek() != '"') {
					str += '"';
					break;
				}
				shiftChar();
			}
			return str;

		case '\\': // Character escape or macro arg
			if (raw)
				break;
			c = peek();
			switch (c) {
			case '\\':
			case '"':
			case '{':
			case '}':
				// Return that character unchanged
				shiftChar();
				break;
			case 'n':
				c = '\n';
				shiftChar();
				break;
			case 'r':
				c = '\r';
				shiftChar();
				break;
			case 't':
				c = '\t';
				shiftChar();
				break;
			case '0':
				c = '\0';
				shiftChar();
				break;

			// Line continuation
			case ' ':
			case '\r':
			case '\n':
				discardLineContinuation();
				continue;

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
				shiftChar();
				if (auto arg = readMacroArg(c); arg) {
					str.append(*arg);
				}
				continue; // Do not copy an additional character

			case EOF: // Can't really print that one
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

			default:
				error("Illegal character escape %s\n", printChar(c));
				shiftChar();
				break;
			}
			break;

		case '{': // Symbol interpolation
			if (raw)
				break;
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			if (auto interpolation = readInterpolation(0); interpolation) {
				str.append(*interpolation);
			}
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

			// Regular characters will just get copied
		}

		str += c;
	}
}

static void appendStringLiteral(std::string &str, bool raw) {
	Defer reenableExpansions = scopedDisableExpansions();

	// We reach this function after reading a single quote, but we also support triple quotes
	bool multiline = false;
	str += '"';
	if (peek() == '"') {
		str += '"';
		shiftChar();
		if (peek() == '"') {
			// """ begins a multi-line string
			str += '"';
			shiftChar();
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			return;
		}
	}

	for (;;) {
		int c = peek();

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			return;
		}

		// We'll be staying in the string, so we can safely consume the char
		shiftChar();

		// Handle '\r' or '\n' (in multiline strings only, already handled above otherwise)
		if (c == '\r' || c == '\n') {
			// Handle CRLF before nextLine() since shiftChar updates colNo
			handleCRLF(c);
			nextLine();
			c = '\n';
		}

		switch (c) {
		case '"':
			if (multiline) {
				// Only """ ends a multi-line string
				if (peek() != '"')
					break;
				str += '"';
				shiftChar();
				if (peek() != '"')
					break;
				str += '"';
				shiftChar();
			}
			str += '"';
			return;

		case '\\': // Character escape or macro arg
			if (raw)
				break;
			c = peek();
			switch (c) {
			// Character escape
			case '\\':
			case '"':
			case '{':
			case '}':
			case 'n':
			case 'r':
			case 't':
			case '0':
				// Return that character unchanged
				str += '\\';
				shiftChar();
				break;

			// Line continuation
			case ' ':
			case '\r':
			case '\n':
				discardLineContinuation();
				continue;

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
			case '<': {
				shiftChar();
				if (auto arg = readMacroArg(c); arg) {
					appendEscapedString(str, *arg);
				}
				continue; // Do not copy an additional character
			}

			case EOF: // Can't really print that one
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

			default:
				error("Illegal character escape %s\n", printChar(c));
				shiftChar();
				break;
			}
			break;

		case '{': // Symbol interpolation
			if (raw)
				break;
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			if (auto interpolation = readInterpolation(0); interpolation) {
				appendEscapedString(str, *interpolation);
			}
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

			// Regular characters will just get copied
		}

		str += c;
	}
}

// Lexer core

static Token yylex_SKIP_TO_ENDC(); // forward declaration for yylex_NORMAL

static Token yylex_NORMAL() {
	for (;;) {
		int c = nextChar();

		switch (c) {
			// Ignore whitespace and comments

		case ';':
			discardComment();
			[[fallthrough]];
		case ' ':
		case '\t':
			break;

			// Handle unambiguous single-char tokens

		case '~':
			return Token(T_(OP_NOT));

		case '@': {
			std::string symName("@");
			return Token(T_(ID), symName);
		}

		case '[':
			return Token(T_(LBRACK));
		case ']':
			return Token(T_(RBRACK));
		case '(':
			return Token(T_(LPAREN));
		case ')':
			return Token(T_(RPAREN));
		case ',':
			return Token(T_(COMMA));

			// Handle ambiguous 1- or 2-char tokens

		case '+': // Either += or ADD
			if (peek() == '=') {
				shiftChar();
				return Token(T_(POP_ADDEQ));
			}
			return Token(T_(OP_ADD));

		case '-': // Either -= or SUB
			if (peek() == '=') {
				shiftChar();
				return Token(T_(POP_SUBEQ));
			}
			return Token(T_(OP_SUB));

		case '*': // Either *=, MUL, or EXP
			switch (peek()) {
			case '=':
				shiftChar();
				return Token(T_(POP_MULEQ));
			case '*':
				shiftChar();
				return Token(T_(OP_EXP));
			default:
				return Token(T_(OP_MUL));
			}

		case '/': // Either /=, DIV, or a block comment
			switch (peek()) {
			case '=':
				shiftChar();
				return Token(T_(POP_DIVEQ));
			case '*':
				shiftChar();
				discardBlockComment();
				break;
			default:
				return Token(T_(OP_DIV));
			}
			break;

		case '|': // Either |=, binary OR, or logical OR
			switch (peek()) {
			case '=':
				shiftChar();
				return Token(T_(POP_OREQ));
			case '|':
				shiftChar();
				return Token(T_(OP_LOGICOR));
			default:
				return Token(T_(OP_OR));
			}

		case '^': // Either ^= or XOR
			if (peek() == '=') {
				shiftChar();
				return Token(T_(POP_XOREQ));
			}
			return Token(T_(OP_XOR));

		case '=': // Either assignment or EQ
			if (peek() == '=') {
				shiftChar();
				return Token(T_(OP_LOGICEQU));
			}
			return Token(T_(POP_EQUAL));

		case '!': // Either a NEQ or negation
			if (peek() == '=') {
				shiftChar();
				return Token(T_(OP_LOGICNE));
			}
			return Token(T_(OP_LOGICNOT));

			// Handle ambiguous 1-, 2-, or 3-char tokens

		case '<': // Either <<=, LT, LTE, or left shift
			switch (peek()) {
			case '=':
				shiftChar();
				return Token(T_(OP_LOGICLE));
			case '<':
				shiftChar();
				if (peek() == '=') {
					shiftChar();
					return Token(T_(POP_SHLEQ));
				}
				return Token(T_(OP_SHL));
			default:
				return Token(T_(OP_LOGICLT));
			}

		case '>': // Either >>=, GT, GTE, or either kind of right shift
			switch (peek()) {
			case '=':
				shiftChar();
				return Token(T_(OP_LOGICGE));
			case '>':
				shiftChar();
				switch (peek()) {
				case '=':
					shiftChar();
					return Token(T_(POP_SHREQ));
				case '>':
					shiftChar();
					return Token(T_(OP_USHR));
				default:
					return Token(T_(OP_SHR));
				}
			default:
				return Token(T_(OP_LOGICGT));
			}

		case ':': // Either :, ::, or an anonymous label ref
			c = peek();
			switch (c) {
			case ':':
				shiftChar();
				return Token(T_(DOUBLE_COLON));
			case '+':
			case '-': {
				std::string symName = readAnonLabelRef(c);
				return Token(T_(ANON), symName);
			}
			default:
				return Token(T_(COLON));
			}

			// Handle numbers

		case '0': // Decimal or fixed-point number
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			uint32_t n = readNumber(10, c - '0');

			if (peek() == '.') {
				shiftChar();
				n = readFractionalPart(n);
			}
			return Token(T_(NUMBER), n);
		}

		case '&': // Either &=, binary AND, logical AND, or an octal constant
			c = peek();
			if (c == '=') {
				shiftChar();
				return Token(T_(POP_ANDEQ));
			} else if (c == '&') {
				shiftChar();
				return Token(T_(OP_LOGICAND));
			} else if (c >= '0' && c <= '7') {
				return Token(T_(NUMBER), readNumber(8, 0));
			}
			return Token(T_(OP_AND));

		case '%': // Either %=, MOD, or a binary constant
			c = peek();
			if (c == '=') {
				shiftChar();
				return Token(T_(POP_MODEQ));
			} else if (c == binDigits[0] || c == binDigits[1]) {
				return Token(T_(NUMBER), readBinaryNumber());
			}
			return Token(T_(OP_MOD));

		case '$': // Hex constant
			return Token(T_(NUMBER), readHexNumber());

		case '`': // Gfx constant
			return Token(T_(NUMBER), readGfxConstant());

			// Handle strings

		case '"':
			return Token(T_(STRING), readString(false));

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
			break;

			// Handle raw strings... or fall through if '#' is not followed by '"'

		case '#':
			if (peek() == '"') {
				shiftChar();
				return Token(T_(STRING), readString(true));
			}
			[[fallthrough]];

			// Handle identifiers... or report garbage characters

		default:
			if (startsIdentifier(c)) {
				Token token = readIdentifier(c);

				// An ELIF after a taken IF needs to not evaluate its condition
				if (token.type == T_(POP_ELIF) && lexerState->lastToken == T_(NEWLINE)
				    && lexer_GetIFDepth() > 0 && lexer_RanIFBlock() && !lexer_ReachedELSEBlock())
					return yylex_SKIP_TO_ENDC();

				// If a keyword, don't try to expand
				if (token.type != T_(ID) && token.type != T_(LOCAL_ID))
					return token;

				// `token` is either an `ID` or a `LOCAL_ID`, and both have a `std::string` value.
				assume(std::holds_alternative<std::string>(token.value));

				// Local symbols cannot be string expansions
				if (token.type == T_(ID) && lexerState->expandStrings) {
					// Attempt string expansion
					Symbol const *sym = sym_FindExactSymbol(std::get<std::string>(token.value));

					if (sym && sym->type == SYM_EQUS) {
						std::shared_ptr<std::string> str = sym->getEqus();

						assume(str);
						beginExpansion(str, sym->name);
						continue; // Restart, reading from the new buffer
					}
				}

				if (token.type == T_(ID) && (lexerState->atLineStart || peek() == ':'))
					token.type = T_(LABEL);

				return token;
			}

			// Do not report weird characters when capturing, it'll be done later
			if (!lexerState->capturing) {
				// TODO: try to group reportings
				error("Unknown character %s\n", printChar(c));
			}
		}
		lexerState->atLineStart = false;
	}
}

static Token yylex_RAW() {
	// This is essentially a modified `appendStringLiteral`
	std::string str;
	size_t parenDepth = 0;
	int c;

	// Trim left whitespace (stops at a block comment)
	for (;;) {
		c = peek();
		if (isWhitespace(c)) {
			shiftChar();
		} else if (c == '\\') {
			shiftChar();
			c = peek();
			// If not a line continuation, handle as a normal char
			if (!isWhitespace(c) && c != '\n' && c != '\r')
				goto backslash;
			// Line continuations count as "whitespace"
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
			appendStringLiteral(str, false);
			break;

		case '#': // Raw string literals inside macro args
			str += c;
			shiftChar();
			if (peek() == '"') {
				shiftChar();
				appendStringLiteral(str, true);
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
			shiftChar();
			if (peek() == '*') {
				shiftChar();
				discardBlockComment();
				continue;
			}
			str += c; // Append the slash
			break;

		case ',': // End of macro arg
			if (parenDepth == 0)
				goto finish;
			goto append;

		case '(': // Open parentheses inside macro args
			if (parenDepth < UINT_MAX)
				parenDepth++;
			goto append;

		case ')': // Close parentheses inside macro args
			if (parenDepth > 0)
				parenDepth--;
			goto append;

		case '\\': // Character escape
			shiftChar();
			c = peek();

backslash:
			switch (c) {
			case ',': // Escapes only valid inside a macro arg
			case '(':
			case ')':
			case '\\': // Escapes shared with string literals
			case '"':
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
			case '\r':
			case '\n':
				discardLineContinuation();
				continue;

			case EOF: // Can't really print that one
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

				// Macro args were already handled by peek, so '\@',
				// '\#', and '\0'-'\9' should not occur here.

			default:
				error("Illegal character escape %s\n", printChar(c));
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

finish:
	// Trim right whitespace
	auto rightPos = std::find_if_not(str.rbegin(), str.rend(), isWhitespace);
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
	if (!str.empty())
		return Token(T_(STRING), str);
	lexer_SetMode(LEXER_NORMAL);

	if (c == '\r' || c == '\n') {
		shiftChar();
		handleCRLF(c);
		return Token(T_(NEWLINE));
	}

	return Token(T_(YYEOF));
}

// This function uses the fact that `if`, etc. constructs are only valid when
// there's nothing before them on their lines. This enables filtering
// "meaningful" (= at line start) vs. "meaningless" (everything else) tokens.
// It's especially important due to macro args not being handled in this
// state, and lexing them in "normal" mode potentially producing such tokens.
static Token skipIfBlock(bool toEndc) {
	lexer_SetMode(LEXER_NORMAL);
	uint32_t startingDepth = lexer_GetIFDepth();

	bool atLineStart = lexerState->atLineStart;
	Defer notAtLineStart{[&] { lexerState->atLineStart = false; }};

	Defer reenableExpansions = scopedDisableExpansions();

	for (;;) {
		if (atLineStart) {
			int c;

			for (;; shiftChar()) {
				c = peek();
				if (!isWhitespace(c))
					break;
			}

			if (startsIdentifier(c)) {
				shiftChar();
				switch (Token token = readIdentifier(c); token.type) {
				case T_(POP_IF):
					lexer_IncIFDepth();
					break;

				case T_(POP_ELIF):
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELIF after an ELSE block\n");
					if (!toEndc && lexer_GetIFDepth() == startingDepth)
						return token;
					break;

				case T_(POP_ELSE):
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELSE after an ELSE block\n");
					lexer_ReachELSEBlock();
					if (!toEndc && lexer_GetIFDepth() == startingDepth)
						return token;
					break;

				case T_(POP_ENDC):
					if (lexer_GetIFDepth() == startingDepth)
						return token;
					lexer_DecIFDepth();
					break;

				default:
					break;
				}
			}
			atLineStart = false;
		}

		// Read chars until EOL
		do {
			int c = nextChar();

			if (c == EOF) {
				return Token(T_(YYEOF));
			} else if (c == '\\') {
				// Unconditionally skip the next char, including line continuations
				c = nextChar();
			} else if (c == '\r' || c == '\n') {
				atLineStart = true;
			}

			if (c == '\r' || c == '\n') {
				// Handle CRLF before nextLine() since shiftChar updates colNo
				handleCRLF(c);
				// Do this both on line continuations and plain EOLs
				nextLine();
			}
		} while (!atLineStart);
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
	int depth = 1;

	bool atLineStart = lexerState->atLineStart;
	Defer notAtLineStart{[&] { lexerState->atLineStart = false; }};

	Defer reenableExpansions = scopedDisableExpansions();

	for (;;) {
		if (atLineStart) {
			int c;

			for (;;) {
				c = peek();
				if (!isWhitespace(c))
					break;
				shiftChar();
			}

			if (startsIdentifier(c)) {
				shiftChar();
				switch (readIdentifier(c).type) {
				case T_(POP_FOR):
				case T_(POP_REPT):
					depth++;
					break;

				case T_(POP_ENDR):
					depth--;
					if (!depth)
						return Token(T_(YYEOF)); // yywrap() will finish the REPT/FOR loop
					break;

				case T_(POP_IF):
					lexer_IncIFDepth();
					break;

				case T_(POP_ENDC):
					lexer_DecIFDepth();
					break;

				default:
					break;
				}
			}
			atLineStart = false;
		}

		// Read chars until EOL
		do {
			int c = nextChar();

			if (c == EOF) {
				return Token(T_(YYEOF));
			} else if (c == '\\') {
				// Unconditionally skip the next char, including line continuations
				c = nextChar();
			} else if (c == '\r' || c == '\n') {
				atLineStart = true;
			}

			if (c == '\r' || c == '\n') {
				// Handle CRLF before nextLine() since shiftChar updates colNo
				handleCRLF(c);
				// Do this both on line continuations and plain EOLs
				nextLine();
			}
		} while (!atLineStart);
	}
}

yy::parser::symbol_type yylex() {
	if (lexerState->atLineStart && lexerStateEOL) {
		lexerState = lexerStateEOL;
		lexerStateEOL = nullptr;
	}
	if (lexerState->lastToken == T_(EOB) && yywrap())
		return yy::parser::make_YYEOF();
	// Newlines read within an expansion should not increase the line count
	if (lexerState->atLineStart && lexerState->expansions.empty())
		nextLine();

	static Token (* const lexerModeFuncs[NB_LEXER_MODES])() = {
	    yylex_NORMAL,
	    yylex_RAW,
	    yylex_SKIP_TO_ELIF,
	    yylex_SKIP_TO_ENDC,
	    yylex_SKIP_TO_ENDR,
	};
	Token token = lexerModeFuncs[lexerState->mode]();

	// Captures end at their buffer's boundary no matter what
	if (token.type == T_(YYEOF) && !lexerState->capturing)
		token = Token(T_(EOB));
	lexerState->lastToken = token.type;
	lexerState->atLineStart = token.type == T_(NEWLINE) || token.type == T_(EOB);

	if (auto *numValue = std::get_if<uint32_t>(&token.value); numValue) {
		return yy::parser::symbol_type(token.type, *numValue);
	} else if (auto *strValue = std::get_if<std::string>(&token.value); strValue) {
		return yy::parser::symbol_type(token.type, *strValue);
	} else {
		assume(std::holds_alternative<std::monostate>(token.value));
		return yy::parser::symbol_type(token.type);
	}
}

static Capture startCapture() {
	// Due to parser internals, it reads the EOL after the expression before calling this.
	// Thus, we don't need to keep one in the buffer afterwards.
	// The following assumption checks that.
	assume(lexerState->atLineStart);

	assume(!lexerState->capturing && lexerState->captureBuf == nullptr);
	lexerState->capturing = true;
	lexerState->captureSize = 0;

	uint32_t lineNo = lexer_GetLineNo();
	if (auto *view = std::get_if<ViewedContent>(&lexerState->content);
	    view && lexerState->expansions.empty()) {
		return {
		    .lineNo = lineNo, .span = {.ptr = view->makeSharedContentPtr(), .size = 0}
        };
	} else {
		assume(lexerState->captureBuf == nullptr);
		lexerState->captureBuf = std::make_shared<std::vector<char>>();
		// `.span.ptr == nullptr`; indicates to retrieve the capture buffer when done capturing
		return {
		    .lineNo = lineNo, .span = {.ptr = nullptr, .size = 0}
        };
	}
}

static void endCapture(Capture &capture) {
	// This being `nullptr` means we're capturing from the capture buffer, which is reallocated
	// during the whole capture process, and so MUST be retrieved at the end
	if (!capture.span.ptr)
		capture.span.ptr = lexerState->makeSharedCaptureBufPtr();
	capture.span.size = lexerState->captureSize;

	// ENDR/ENDM or EOF puts us past the start of the line
	lexerState->atLineStart = false;

	lexerState->capturing = false;
	lexerState->captureBuf = nullptr;
}

Capture lexer_CaptureRept() {
	Capture capture = startCapture();

	Defer reenableExpansions = scopedDisableExpansions();

	size_t depth = 0;
	int c = EOF;

	for (;;) {
		nextLine();
		// We're at line start, so attempt to match a `REPT` or `ENDR` token
		do { // Discard initial whitespace
			c = nextChar();
		} while (isWhitespace(c));
		// Now, try to match `REPT`, `FOR` or `ENDR` as a **whole** identifier
		if (startsIdentifier(c)) {
			switch (readIdentifier(c).type) {
			case T_(POP_REPT):
			case T_(POP_FOR):
				depth++;
				// Ignore the rest of that line
				break;

			case T_(POP_ENDR):
				if (!depth) {
					endCapture(capture);
					// The final ENDR has been captured, but we don't want it!
					// We know we have read exactly "ENDR", not e.g. an EQUS
					capture.span.size -= QUOTEDSTRLEN("ENDR");
					return capture;
				}
				depth--;
				break;

			default:
				break;
			}
		}

		// Just consume characters until EOL or EOF
		for (;; c = nextChar()) {
			if (c == EOF) {
				error("Unterminated REPT/FOR block\n");
				endCapture(capture);
				capture.span.ptr = nullptr; // Indicates that it reached EOF before an ENDR
				return capture;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
		}
	}
}

Capture lexer_CaptureMacro() {
	Capture capture = startCapture();

	Defer reenableExpansions = scopedDisableExpansions();

	int c = EOF;

	for (;;) {
		nextLine();
		// We're at line start, so attempt to match an `ENDM` token
		do { // Discard initial whitespace
			c = nextChar();
		} while (isWhitespace(c));
		// Now, try to match `ENDM` as a **whole** identifier
		if (startsIdentifier(c)) {
			switch (readIdentifier(c).type) {
			case T_(POP_ENDM):
				endCapture(capture);
				// The ENDM has been captured, but we don't want it!
				// We know we have read exactly "ENDM", not e.g. an EQUS
				capture.span.size -= QUOTEDSTRLEN("ENDM");
				return capture;

			default:
				break;
			}
		}

		// Just consume characters until EOL or EOF
		for (;; c = nextChar()) {
			if (c == EOF) {
				error("Unterminated macro definition\n");
				endCapture(capture);
				capture.span.ptr = nullptr; // Indicates that it reached EOF before an ENDM
				return capture;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
		}
	}
}
