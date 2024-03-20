/* SPDX-License-Identifier: MIT */

#include "asm/lexer.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#ifndef _MSC_VER
	#include <unistd.h>
#endif

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

	#define MAP_FAILED nullptr

static void mapFile(void *&mappingAddr, int fd, std::string const &path, size_t) {
	mappingAddr = MAP_FAILED;
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
}

static int munmap(void *mappingAddr, size_t) {
	return UnmapViewOfFile(mappingAddr) == 0 ? -1 : 0;
}

#else // defined(_MSC_VER) || defined(__MINGW32__)
	#include <sys/mman.h>

static void mapFile(void *&mappingAddr, int fd, std::string const &path, size_t size) {
	mappingAddr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mappingAddr == MAP_FAILED && errno == ENOTSUP) {
		// The implementation may not support MAP_PRIVATE; try again with MAP_SHARED
		// instead, offering, I believe, weaker guarantees about external modifications to
		// the file while reading it. That's still better than not opening it at all, though.
		if (verbose)
			printf("mmap(%s, MAP_PRIVATE) failed, retrying with MAP_SHARED\n", path.c_str());
		mappingAddr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	}
}

#endif // !( defined(_MSC_VER) || defined(__MINGW32__) )

// Bison 3.6 changed token "types" to "kinds"; cast to int for simple compatibility
#define T_(name) (int)yy::parser::token::name

struct Token {
	int type;
	std::variant<std::monostate, uint32_t, String, std::string> value;

	Token() : type(T_(NUMBER)), value(std::monostate{}) {}
	Token(int type_) : type(type_), value(std::monostate{}) {}
	Token(int type_, uint32_t value_) : type(type_), value(value_) {}
	Token(int type_, String &value_) : type(type_), value(value_) {}
	Token(int type_, std::string &value_) : type(type_), value(value_) {}
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

LexerState *lexerState = nullptr;
LexerState *lexerStateEOL = nullptr;

static void initState(LexerState &state) {
	state.mode = LEXER_NORMAL;
	state.atLineStart = true; // yylex() will init colNo due to this
	state.lastToken = T_(YYEOF);

	state.ifStack.clear();

	state.capturing = false;
	state.captureBuf = nullptr;

	state.disableMacroArgs = false;
	state.disableInterpolation = false;
	state.macroArgScanDistance = 0;
	state.expandStrings = true;

	state.expansions.clear();
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

bool lexer_OpenFile(LexerState &state, std::string const &path) {
	if (path == "-") {
		state.path = "<stdin>";
		state.content = BufferedLexerState{.fd = STDIN_FILENO, .index = 0, .buf = {}, .nbChars = 0};
		if (verbose)
			printf("Opening stdin\n");
	} else {
		struct stat fileInfo;
		if (stat(path.c_str(), &fileInfo) != 0) {
			error("Failed to stat file \"%s\": %s\n", path.c_str(), strerror(errno));
			return false;
		}
		state.path = path;

		int fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			error("Failed to open file \"%s\": %s\n", path.c_str(), strerror(errno));
			return false;
		}

		bool isMmapped = false;

		if (fileInfo.st_size > 0) {
			// Try using `mmap` for better performance
			void *mappingAddr;
			mapFile(mappingAddr, fd, path, fileInfo.st_size);

			if (mappingAddr != MAP_FAILED) {
				close(fd);
				state.content = MmappedLexerState{
				    .ptr = (char *)mappingAddr,
				    .size = (size_t)fileInfo.st_size,
				    .offset = 0,
				    .isReferenced = false,
				};
				if (verbose)
					printf("File \"%s\" is mmap()ped\n", path.c_str());
				isMmapped = true;
			}
		}

		if (!isMmapped) {
			// Sometimes mmap() fails or isn't available, so have a fallback
			state.content = BufferedLexerState{.fd = fd, .index = 0, .buf = {}, .nbChars = 0};
			if (verbose) {
				if (fileInfo.st_size == 0) {
					printf("File \"%s\" is empty\n", path.c_str());
				} else {
					printf(
					    "File \"%s\" is opened; errno reports: %s\n", path.c_str(), strerror(errno)
					);
				}
			}
		}
	}

	initState(state);
	state.lineNo = 0; // Will be incremented at first line start
	return true;
}

void lexer_OpenFileView(
    LexerState &state, char const *path, char const *buf, size_t size, uint32_t lineNo
) {
	state.path = path; // Used to report read errors in `peekInternal`
	state.content = ViewedLexerState{.ptr = buf, .size = size, .offset = 0};
	initState(state);
	state.lineNo = lineNo; // Will be incremented at first line start
}

void lexer_RestartRept(uint32_t lineNo) {
	if (auto *mmap = std::get_if<MmappedLexerState>(&lexerState->content); mmap) {
		mmap->offset = 0;
	} else if (auto *view = std::get_if<ViewedLexerState>(&lexerState->content); view) {
		view->offset = 0;
	}
	initState(*lexerState);
	lexerState->lineNo = lineNo;
}

void lexer_CleanupState(LexerState &state) {
	// A big chunk of the lexer state soundness is the file stack ("fstack").
	// Each context in the fstack has its own *unique* lexer state; thus, we always guarantee
	// that lexer states lifetimes are always properly managed, since they're handled solely
	// by the fstack... with *one* exception.
	// Assume a context is pushed on top of the fstack, and the corresponding lexer state gets
	// scheduled at EOF; `lexerStateEOL` thus becomes a (weak) ref to that lexer state...
	// It has been possible, due to a bug, that the corresponding fstack context gets popped
	// before EOL, deleting the associated state... but it would still be switched to at EOL.
	// This assertion checks that this doesn't happen again.
	// It could be argued that deleting a state that's scheduled for EOF could simply clear
	// `lexerStateEOL`, but there's currently no situation in which this should happen.
	assert(&state != lexerStateEOL);

	if (auto *mmap = std::get_if<MmappedLexerState>(&state.content); mmap) {
		if (!mmap->isReferenced)
			munmap(mmap->ptr, mmap->size);
	} else if (auto *cbuf = std::get_if<BufferedLexerState>(&state.content); cbuf) {
		close(cbuf->fd);
	}
}

void lexer_SetMode(LexerMode mode) {
	lexerState->mode = mode;
}

void lexer_ToggleStringExpansion(bool enable) {
	lexerState->expandStrings = enable;
}

// Functions for the actual lexer to obtain characters

static void beginExpansion(char const *str, bool owned, char const *name) {
	size_t size = strlen(str);

	// Do not expand empty strings
	if (!size)
		return;

	if (name)
		lexer_CheckRecursionDepth();

	lexerState->expansions.push_front({
	    .name = name ? std::optional<std::string>(name) : std::nullopt,
	    .contents = {.unowned = str},
	    .size = size,
	    .offset = 0,
	    .owned = owned,
	});
}

void lexer_CheckRecursionDepth() {
	if (lexerState->expansions.size() > maxRecursionDepth + 1)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);
}

static void freeExpansion(Expansion &expansion) {
	if (expansion.owned)
		delete[] expansion.contents.owned;
}

static bool isMacroChar(char c) {
	return c == '@' || c == '#' || c == '<' || (c >= '0' && c <= '9');
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
	}

	lexerState->disableMacroArgs = disableMacroArgs;
	lexerState->disableInterpolation = disableInterpolation;
	return num;
}

static char const *readMacroArg(char name) {
	char const *str = nullptr;

	if (name == '@') {
		auto maybeStr = fstk_GetUniqueIDStr();
		str = maybeStr ? maybeStr->c_str() : nullptr;
	} else if (name == '#') {
		str = macro_GetAllArgs();
	} else if (name == '<') {
		uint32_t num = readBracketedMacroArgNum();

		if (num == 0)
			return nullptr;
		str = macro_GetArg(num);
		if (!str)
			error("Macro argument '\\<%" PRIu32 ">' not defined\n", num);
		return str;
	} else if (name == '0') {
		error("Invalid macro argument '\\0'\n");
		return nullptr;
	} else {
		assert(name > '0' && name <= '9');
		str = macro_GetArg(name - '0');
	}

	if (!str)
		error("Macro argument '\\%c' not defined\n", name);
	return str;
}

static size_t readInternal(BufferedLexerState &cbuf, size_t bufIndex, size_t nbChars) {
	// This buffer overflow made me lose WEEKS of my life. Never again.
	assert(bufIndex + nbChars <= LEXER_BUF_SIZE);
	ssize_t nbReadChars = read(cbuf.fd, &cbuf.buf[bufIndex], nbChars);

	if (nbReadChars == -1)
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path.c_str(), strerror(errno));

	// `nbReadChars` cannot be negative, so it's fine to cast to `size_t`
	return (size_t)nbReadChars;
}

// We only need one character of lookahead, for macro arguments
static int peekInternal(uint8_t distance) {
	for (Expansion &exp : lexerState->expansions) {
		// An expansion that has reached its end will have `exp->offset` == `exp->size`,
		// and `peekInternal` will continue with its parent
		assert(exp.offset <= exp.size);
		if (distance < exp.size - exp.offset)
			return exp.contents.unowned[exp.offset + distance];
		distance -= exp.size - exp.offset;
	}

	if (distance >= LEXER_BUF_SIZE)
		fatalerror(
		    "Internal lexer error: buffer has insufficient size for peeking (%" PRIu8 " >= %u)\n",
		    distance,
		    LEXER_BUF_SIZE
		);

	if (auto *mmap = std::get_if<MmappedLexerState>(&lexerState->content); mmap) {
		if (size_t idx = mmap->offset + distance; idx < mmap->size)
			return (uint8_t)mmap->ptr[idx];
		return EOF;
	} else if (auto *view = std::get_if<ViewedLexerState>(&lexerState->content); view) {
		if (size_t idx = view->offset + distance; idx < view->size)
			return (uint8_t)view->ptr[idx];
		return EOF;
	} else {
		assert(std::holds_alternative<BufferedLexerState>(lexerState->content));
		auto &cbuf = std::get<BufferedLexerState>(lexerState->content);

		if (cbuf.nbChars > distance)
			return (uint8_t)cbuf.buf[(cbuf.index + distance) % LEXER_BUF_SIZE];

		// Buffer isn't full enough, read some chars in
		size_t target = LEXER_BUF_SIZE - cbuf.nbChars; // Aim: making the buf full

		// Compute the index we'll start writing to
		size_t writeIndex = (cbuf.index + cbuf.nbChars) % LEXER_BUF_SIZE;

		// If the range to fill passes over the buffer wrapping point, we need two reads
		if (writeIndex + target > LEXER_BUF_SIZE) {
			size_t nbExpectedChars = LEXER_BUF_SIZE - writeIndex;
			size_t nbReadChars = readInternal(cbuf, writeIndex, nbExpectedChars);

			cbuf.nbChars += nbReadChars;

			writeIndex += nbReadChars;
			if (writeIndex == LEXER_BUF_SIZE)
				writeIndex = 0;

			// If the read was incomplete, don't perform a second read
			target -= nbReadChars;
			if (nbReadChars < nbExpectedChars)
				target = 0;
		}
		if (target != 0)
			cbuf.nbChars += readInternal(cbuf, writeIndex, target);

		if (cbuf.nbChars > distance)
			return (uint8_t)cbuf.buf[(cbuf.index + distance) % LEXER_BUF_SIZE];

		// If there aren't enough chars even after refilling, give up
		return EOF;
	}
}

// forward declarations for peek
static void shiftChar();
static char const *readInterpolation(size_t depth);

static int peek() {
	int c = peekInternal(0);

	if (lexerState->macroArgScanDistance > 0)
		return c;

	lexerState->macroArgScanDistance++; // Do not consider again

	if (c == '\\' && !lexerState->disableMacroArgs) {
		// If character is a backslash, check for a macro arg
		lexerState->macroArgScanDistance++;
		c = peekInternal(1);
		if (isMacroChar(c)) {
			shiftChar();
			shiftChar();
			char const *str = readMacroArg(c);

			// If the macro arg is invalid or an empty string, it cannot be
			// expanded, so skip it and keep peeking.
			if (!str || !str[0])
				return peek();

			beginExpansion(str, c == '#', nullptr);

			// Assuming macro args can't be recursive (I'll be damned if a way
			// is found...), then we mark the entire macro arg as scanned.
			lexerState->macroArgScanDistance += strlen(str);

			c = str[0];
		} else {
			c = '\\';
		}
	} else if (c == '{' && !lexerState->disableInterpolation) {
		// If character is an open brace, do symbol interpolation
		shiftChar();
		char const *str = readInterpolation(0);

		if (str && str[0])
			beginExpansion(str, false, str);
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
		Expansion &expansion = lexerState->expansions.front();

		assert(expansion.offset <= expansion.size);
		expansion.offset++;
		if (expansion.offset > expansion.size) {
			// When advancing would go past an expansion's end, free it,
			// move up to its parent, and try again to advance
			freeExpansion(expansion);
			lexerState->expansions.pop_front();
			goto restart;
		}
	} else {
		// Advance within the file contents
		lexerState->colNo++;
		if (auto *mmap = std::get_if<MmappedLexerState>(&lexerState->content); mmap) {
			mmap->offset++;
		} else if (auto *view = std::get_if<ViewedLexerState>(&lexerState->content); view) {
			view->offset++;
		} else {
			assert(std::holds_alternative<BufferedLexerState>(lexerState->content));
			auto &cbuf = std::get<BufferedLexerState>(lexerState->content);
			assert(cbuf.index < LEXER_BUF_SIZE);
			cbuf.index++;
			if (cbuf.index == LEXER_BUF_SIZE)
				cbuf.index = 0; // Wrap around if necessary
			assert(cbuf.nbChars > 0);
			cbuf.nbChars--;
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
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	for (;;) {
		int c = nextChar();

		switch (c) {
		case EOF:
			error("Unterminated block comment\n");
			goto finish;
		case '\r':
			// Handle CRLF before nextLine() since shiftChar updates colNo
			handleCRLF(c);
			// fallthrough
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
				goto finish;
			}
			// fallthrough
		default:
			continue;
		}
	}
finish:
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

static void discardComment() {
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	for (;; shiftChar()) {
		int c = peek();

		if (c == EOF || c == '\r' || c == '\n')
			break;
	}
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
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

static char const *readInterpolation(size_t depth) {
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
			char const *str = readInterpolation(depth + 1);

			if (str && str[0])
				beginExpansion(str, false, str);
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

	static char buf[MAXSTRLEN + 1];

	Symbol const *sym = sym_FindScopedValidSymbol(fmtBuf);

	if (!sym) {
		error("Interpolated symbol \"%s\" does not exist\n", fmtBuf.c_str());
	} else if (sym->type == SYM_EQUS) {
		std::string str = fmt.formatString(*sym->getEqus());
		memcpy(buf, str.c_str(), str.length() + 1);
		return buf;
	} else if (sym->isNumeric()) {
		std::string str = fmt.formatNumber(sym->getConstantValue());
		memcpy(buf, str.c_str(), str.length() + 1);
		return buf;
	} else {
		error("Only numerical and string symbols can be interpolated\n");
	}
	return nullptr;
}

#define append_yylval_string(c) \
	do { \
		/* Evaluate c exactly once in case it has side effects */ \
		if (char v = (c); i < sizeof(yylval.string)) \
			yylval.string[i++] = v; \
	} while (0)

static size_t appendEscapedSubstring(String &yylval, char const *str, size_t i) {
	// Copy one extra to flag overflow
	while (*str) {
		char c = *str++;

		// Escape characters that need escaping
		switch (c) {
		case '\\':
		case '"':
		case '{':
			append_yylval_string('\\');
			break;
		case '\n':
			append_yylval_string('\\');
			c = 'n';
			break;
		case '\r':
			append_yylval_string('\\');
			c = 'r';
			break;
		case '\t':
			append_yylval_string('\\');
			c = 't';
			break;
		}

		append_yylval_string(c);
	}

	return i;
}

static void readString(String &yylval, bool raw) {
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	size_t i = 0;
	bool multiline = false;
	char const *str;

	// We reach this function after reading a single quote, but we also support triple quotes
	if (peek() == '"') {
		shiftChar();
		if (peek() == '"') {
			// """ begins a multi-line string
			shiftChar();
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			goto finish;
		}
	}

	for (;;) {
		int c = peek();

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			break;
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
					append_yylval_string('"');
					break;
				}
				shiftChar();
			}
			goto finish;

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

			// Line continuation
			case ' ':
			case '\r':
			case '\n':
				discardLineContinuation();
				continue;

			// Macro arg
			case '@':
			case '#':
			case '0':
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
				str = readMacroArg(c);
				if (str) {
					while (*str)
						append_yylval_string(*str++);
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
			str = readInterpolation(0);
			if (str) {
				while (*str)
					append_yylval_string(*str++);
			}
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

			// Regular characters will just get copied
		}

		append_yylval_string(c);
	}

finish:
	if (i == sizeof(yylval.string)) {
		i--;
		warning(WARNING_LONG_STR, "String constant too long\n");
	}
	yylval.string[i] = '\0';

	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

static size_t appendStringLiteral(String &yylval, size_t i, bool raw) {
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	bool multiline = false;
	char const *str;

	// We reach this function after reading a single quote, but we also support triple quotes
	append_yylval_string('"');
	if (peek() == '"') {
		append_yylval_string('"');
		shiftChar();
		if (peek() == '"') {
			// """ begins a multi-line string
			append_yylval_string('"');
			shiftChar();
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			goto finish;
		}
	}

	for (;;) {
		int c = peek();

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			break;
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
				append_yylval_string('"');
				shiftChar();
				if (peek() != '"')
					break;
				append_yylval_string('"');
				shiftChar();
			}
			append_yylval_string('"');
			goto finish;

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
				// Return that character unchanged
				append_yylval_string('\\');
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
			case '0':
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
				str = readMacroArg(c);
				if (str && str[0])
					i = appendEscapedSubstring(yylval, str, i);
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
			str = readInterpolation(0);
			if (str && str[0])
				i = appendEscapedSubstring(yylval, str, i);
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

			// Regular characters will just get copied
		}

		append_yylval_string(c);
	}

finish:
	if (i == sizeof(yylval.string)) {
		i--;
		warning(WARNING_LONG_STR, "String constant too long\n");
	}
	yylval.string[i] = '\0';

	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;

	return i;
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
			// fallthrough
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

		case '"': {
			String yylval;
			readString(yylval, false);
			return Token(T_(STRING), yylval);
		}

			// Handle newlines and EOF

		case '\r':
			handleCRLF(c);
			// fallthrough
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
				String yylval;
				readString(yylval, true);
				return Token(T_(STRING), yylval);
			}
			// fallthrough

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
				assert(std::holds_alternative<std::string>(token.value));

				// Local symbols cannot be string expansions
				if (token.type == T_(ID) && lexerState->expandStrings) {
					// Attempt string expansion
					Symbol const *sym = sym_FindExactSymbol(std::get<std::string>(token.value));

					if (sym && sym->type == SYM_EQUS) {
						char const *str = sym->getEqus()->c_str();

						assert(str);
						if (str[0])
							beginExpansion(str, false, sym->name.c_str());
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
	String yylval;
	size_t parenDepth = 0;
	size_t i = 0;
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
			i = appendStringLiteral(yylval, i, false);
			break;

		case '#': // Raw string literals inside macro args
			append_yylval_string(c);
			shiftChar();
			if (peek() == '"') {
				shiftChar();
				i = appendStringLiteral(yylval, i, true);
			}
			break;

		case ';': // Comments inside macro args
			discardComment();
			c = peek();
			// fallthrough
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
			append_yylval_string(c); // Append the slash
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
			// fallthrough

		default: // Regular characters will just get copied
append:
			append_yylval_string(c);
			shiftChar();
			break;
		}
	}

finish:
	if (i == sizeof(yylval.string)) {
		i--;
		warning(WARNING_LONG_STR, "Macro argument too long\n");
	}
	// Trim right whitespace
	while (i && isWhitespace(yylval.string[i - 1]))
		i--;
	yylval.string[i] = '\0';

	// Returning COMMAs to the parser would mean that two consecutive commas
	// (i.e. an empty argument) need to return two different tokens (STRING
	// then COMMA) without advancing the read. To avoid this, commas in raw
	// mode end the current macro argument but are not tokenized themselves.
	if (c == ',') {
		shiftChar();
		return Token(T_(STRING), yylval);
	}

	// The last argument may end in a trailing comma, newline, or EOF.
	// To allow trailing commas, raw mode will continue after the last
	// argument, immediately lexing the newline or EOF again (i.e. with
	// an empty raw string before it). This will not be treated as a
	// macro argument. To pass an empty last argument, use a second
	// trailing comma.
	if (i > 0)
		return Token(T_(STRING), yylval);
	lexer_SetMode(LEXER_NORMAL);

	if (c == '\r' || c == '\n') {
		shiftChar();
		handleCRLF(c);
		return Token(T_(NEWLINE));
	}

	return Token(T_(YYEOF));
}

#undef append_yylval_string

// This function uses the fact that `if`, etc. constructs are only valid when
// there's nothing before them on their lines. This enables filtering
// "meaningful" (= at line start) vs. "meaningless" (everything else) tokens.
// It's especially important due to macro args not being handled in this
// state, and lexing them in "normal" mode potentially producing such tokens.
static Token skipIfBlock(bool toEndc) {
	lexer_SetMode(LEXER_NORMAL);
	uint32_t startingDepth = lexer_GetIFDepth();
	Token token;
	bool atLineStart = lexerState->atLineStart;

	// Prevent expanding macro args and symbol interpolation in this state
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

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
				token = readIdentifier(c);
				switch (token.type) {
				case T_(POP_IF):
					lexer_IncIFDepth();
					break;

				case T_(POP_ELIF):
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELIF after an ELSE block\n");
					if (!toEndc && lexer_GetIFDepth() == startingDepth)
						goto finish;
					break;

				case T_(POP_ELSE):
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELSE after an ELSE block\n");
					lexer_ReachELSEBlock();
					if (!toEndc && lexer_GetIFDepth() == startingDepth)
						goto finish;
					break;

				case T_(POP_ENDC):
					if (lexer_GetIFDepth() == startingDepth)
						goto finish;
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
				token = Token(T_(YYEOF));
				goto finish;
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

finish:
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	lexerState->atLineStart = false;

	return token;
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

	// Prevent expanding macro args and symbol interpolation in this state
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

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
						goto finish;
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
				goto finish;
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

finish:
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	lexerState->atLineStart = false;

	// yywrap() will finish the REPT/FOR loop
	return Token(T_(YYEOF));
}

yy::parser::symbol_type yylex() {
	if (lexerState->atLineStart && lexerStateEOL) {
		lexer_SetState(lexerStateEOL);
		lexerStateEOL = nullptr;
	}
	// `lexer_SetState` updates `lexerState`, so check for EOF after it
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
	} else if (auto *stringValue = std::get_if<String>(&token.value); stringValue) {
		return yy::parser::symbol_type(token.type, *stringValue);
	} else if (auto *strValue = std::get_if<std::string>(&token.value); strValue) {
		return yy::parser::symbol_type(token.type, *strValue);
	} else {
		assert(std::holds_alternative<std::monostate>(token.value));
		return yy::parser::symbol_type(token.type);
	}
}

static void startCapture(CaptureBody &capture) {
	assert(!lexerState->capturing);
	lexerState->capturing = true;
	lexerState->captureSize = 0;
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	capture.lineNo = lexer_GetLineNo();
	if (auto *mmap = std::get_if<MmappedLexerState>(&lexerState->content);
	    mmap && lexerState->expansions.empty()) {
		capture.body = &mmap->ptr[mmap->offset];
	} else if (auto *view = std::get_if<ViewedLexerState>(&lexerState->content);
	           view && lexerState->expansions.empty()) {
		capture.body = &view->ptr[view->offset];
	} else {
		capture.body = nullptr; // Indicates to retrieve the capture buffer when done capturing
		assert(lexerState->captureBuf == nullptr);
		lexerState->captureBuf = new (std::nothrow) std::vector<char>();
		if (!lexerState->captureBuf)
			fatalerror("Failed to allocate capture buffer: %s\n", strerror(errno));
	}
}

static void endCapture(CaptureBody &capture) {
	// This being `nullptr` means we're capturing from the capture buf, which is reallocated
	// during the whole capture process, and so MUST be retrieved at the end
	if (!capture.body)
		capture.body = lexerState->captureBuf->data();
	capture.size = lexerState->captureSize;

	lexerState->capturing = false;
	lexerState->captureBuf = nullptr;
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

bool lexer_CaptureRept(CaptureBody &capture) {
	startCapture(capture);

	size_t depth = 0;
	int c = EOF;

	// Due to parser internals, it reads the EOL after the expression before calling this.
	// Thus, we don't need to keep one in the buffer afterwards.
	// The following assertion checks that.
	assert(lexerState->atLineStart);
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
					// The final ENDR has been captured, but we don't want it!
					// We know we have read exactly "ENDR", not e.g. an EQUS
					lexerState->captureSize -= strlen("ENDR");
					goto finish;
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
				goto finish;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
		}
	}

finish:
	endCapture(capture);
	// ENDR or EOF puts us past the start of the line
	lexerState->atLineStart = false;

	// Returns true if an ENDR terminated the block, false if it reached EOF first
	return c != EOF;
}

bool lexer_CaptureMacroBody(CaptureBody &capture) {
	startCapture(capture);

	// If the file is `mmap`ed, we need not to unmap it to keep access to the macro
	if (auto *mmap = std::get_if<MmappedLexerState>(&lexerState->content); mmap)
		mmap->isReferenced = true;

	int c = EOF;

	// Due to parser internals, it reads the EOL after the expression before calling this.
	// Thus, we don't need to keep one in the buffer afterwards.
	// The following assertion checks that.
	assert(lexerState->atLineStart);
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
				// The ENDM has been captured, but we don't want it!
				// We know we have read exactly "ENDM", not e.g. an EQUS
				lexerState->captureSize -= strlen("ENDM");
				goto finish;

			default:
				break;
			}
		}

		// Just consume characters until EOL or EOF
		for (;; c = nextChar()) {
			if (c == EOF) {
				error("Unterminated macro definition\n");
				goto finish;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
		}
	}

finish:
	endCapture(capture);
	// ENDM or EOF puts us past the start of the line
	lexerState->atLineStart = false;

	// Returns true if an ENDM terminated the block, false if it reached EOF first
	return c != EOF;
}
