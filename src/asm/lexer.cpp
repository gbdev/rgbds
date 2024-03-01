/* SPDX-License-Identifier: MIT */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "platform.hpp" // For `ssize_t` and `AT`
#include "util.hpp"

#include "asm/lexer.hpp"
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
# define WIN32_LEAN_AND_MEAN // include less from windows.h
# include <windows.h> // target architecture
# include <fileapi.h> // CreateFileA
# include <winbase.h> // CreateFileMappingA
# include <memoryapi.h> // MapViewOfFile
# include <handleapi.h> // CloseHandle
# define MAP_FAILED nullptr
# define mapFile(ptr, fd, path, size) do { \
	(ptr) = MAP_FAILED; \
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, \
				  FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_RANDOM_ACCESS, nullptr); \
	HANDLE mappingObj; \
	\
	if (file == INVALID_HANDLE_VALUE) \
		break; \
	mappingObj  = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr); \
	if (mappingObj != INVALID_HANDLE_VALUE) \
		(ptr) = MapViewOfFile(mappingObj, FILE_MAP_READ, 0, 0, 0); \
	CloseHandle(mappingObj); \
	CloseHandle(file); \
} while (0)
# define munmap(ptr, size)  UnmapViewOfFile((ptr))

#else // defined(_MSC_VER) || defined(__MINGW32__)

# include <sys/mman.h>
# define mapFile(ptr, fd, path, size) do { \
	(ptr) = mmap(nullptr, (size), PROT_READ, MAP_PRIVATE, (fd), 0); \
	\
	if ((ptr) == MAP_FAILED && errno == ENOTSUP) { \
		/*
		 * The implementation may not support MAP_PRIVATE; try again with MAP_SHARED
		 * instead, offering, I believe, weaker guarantees about external modifications to
		 * the file while reading it. That's still better than not opening it at all, though
		 */ \
		if (verbose) \
			printf("mmap(%s, MAP_PRIVATE) failed, retrying with MAP_SHARED\n", path); \
		(ptr) = mmap(nullptr, (size), PROT_READ, MAP_SHARED, (fd), 0); \
	} \
} while (0)
#endif // !( defined(_MSC_VER) || defined(__MINGW32__) )

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
	{"ADC", T_Z80_ADC},
	{"ADD", T_Z80_ADD},
	{"AND", T_Z80_AND},
	{"BIT", T_Z80_BIT},
	{"CALL", T_Z80_CALL},
	{"CCF", T_Z80_CCF},
	{"CPL", T_Z80_CPL},
	{"CP", T_Z80_CP},
	{"DAA", T_Z80_DAA},
	{"DEC", T_Z80_DEC},
	{"DI", T_Z80_DI},
	{"EI", T_Z80_EI},
	{"HALT", T_Z80_HALT},
	{"INC", T_Z80_INC},
	{"JP", T_Z80_JP},
	{"JR", T_Z80_JR},
	{"LD", T_Z80_LD},
	{"LDI", T_Z80_LDI},
	{"LDD", T_Z80_LDD},
	{"LDIO", T_Z80_LDH},
	{"LDH", T_Z80_LDH},
	{"NOP", T_Z80_NOP},
	{"OR", T_Z80_OR},
	{"POP", T_Z80_POP},
	{"PUSH", T_Z80_PUSH},
	{"RES", T_Z80_RES},
	{"RETI", T_Z80_RETI},
	{"RET", T_Z80_RET},
	{"RLCA", T_Z80_RLCA},
	{"RLC", T_Z80_RLC},
	{"RLA", T_Z80_RLA},
	{"RL", T_Z80_RL},
	{"RRC", T_Z80_RRC},
	{"RRCA", T_Z80_RRCA},
	{"RRA", T_Z80_RRA},
	{"RR", T_Z80_RR},
	{"RST", T_Z80_RST},
	{"SBC", T_Z80_SBC},
	{"SCF", T_Z80_SCF},
	{"SET", T_Z80_SET},
	{"SLA", T_Z80_SLA},
	{"SRA", T_Z80_SRA},
	{"SRL", T_Z80_SRL},
	{"STOP", T_Z80_STOP},
	{"SUB", T_Z80_SUB},
	{"SWAP", T_Z80_SWAP},
	{"XOR", T_Z80_XOR},

	{"NZ", T_CC_NZ},
	{"Z", T_CC_Z},
	{"NC", T_CC_NC},
	// Handled after as T_TOKEN_C
	// { "C", T_CC_C },

	{"AF", T_MODE_AF},
	{"BC", T_MODE_BC},
	{"DE", T_MODE_DE},
	{"HL", T_MODE_HL},
	{"SP", T_MODE_SP},
	{"HLD", T_MODE_HL_DEC},
	{"HLI", T_MODE_HL_INC},

	{"A", T_TOKEN_A},
	{"B", T_TOKEN_B},
	{"C", T_TOKEN_C},
	{"D", T_TOKEN_D},
	{"E", T_TOKEN_E},
	{"H", T_TOKEN_H},
	{"L", T_TOKEN_L},

	{"DEF", T_OP_DEF},

	{"FRAGMENT", T_POP_FRAGMENT},
	{"BANK", T_OP_BANK},
	{"ALIGN", T_OP_ALIGN},

	{"SIZEOF", T_OP_SIZEOF},
	{"STARTOF", T_OP_STARTOF},

	{"ROUND", T_OP_ROUND},
	{"CEIL", T_OP_CEIL},
	{"FLOOR", T_OP_FLOOR},
	{"DIV", T_OP_FDIV},
	{"MUL", T_OP_FMUL},
	{"FMOD", T_OP_FMOD},
	{"POW", T_OP_POW},
	{"LOG", T_OP_LOG},
	{"SIN", T_OP_SIN},
	{"COS", T_OP_COS},
	{"TAN", T_OP_TAN},
	{"ASIN", T_OP_ASIN},
	{"ACOS", T_OP_ACOS},
	{"ATAN", T_OP_ATAN},
	{"ATAN2", T_OP_ATAN2},

	{"HIGH", T_OP_HIGH},
	{"LOW", T_OP_LOW},
	{"ISCONST", T_OP_ISCONST},

	{"STRCMP", T_OP_STRCMP},
	{"STRIN", T_OP_STRIN},
	{"STRRIN", T_OP_STRRIN},
	{"STRSUB", T_OP_STRSUB},
	{"STRLEN", T_OP_STRLEN},
	{"STRCAT", T_OP_STRCAT},
	{"STRUPR", T_OP_STRUPR},
	{"STRLWR", T_OP_STRLWR},
	{"STRRPL", T_OP_STRRPL},
	{"STRFMT", T_OP_STRFMT},

	{"CHARLEN", T_OP_CHARLEN},
	{"CHARSUB", T_OP_CHARSUB},
	{"INCHARMAP", T_OP_INCHARMAP},

	{"INCLUDE", T_POP_INCLUDE},
	{"PRINT", T_POP_PRINT},
	{"PRINTLN", T_POP_PRINTLN},
	{"EXPORT", T_POP_EXPORT},
	{"DS", T_POP_DS},
	{"DB", T_POP_DB},
	{"DW", T_POP_DW},
	{"DL", T_POP_DL},
	{"SECTION", T_POP_SECTION},
	{"ENDSECTION", T_POP_ENDSECTION},
	{"PURGE", T_POP_PURGE},

	{"RSRESET", T_POP_RSRESET},
	{"RSSET", T_POP_RSSET},

	{"INCBIN", T_POP_INCBIN},
	{"CHARMAP", T_POP_CHARMAP},
	{"NEWCHARMAP", T_POP_NEWCHARMAP},
	{"SETCHARMAP", T_POP_SETCHARMAP},
	{"PUSHC", T_POP_PUSHC},
	{"POPC", T_POP_POPC},

	{"FAIL", T_POP_FAIL},
	{"WARN", T_POP_WARN},
	{"FATAL", T_POP_FATAL},
	{"ASSERT", T_POP_ASSERT},
	{"STATIC_ASSERT", T_POP_STATIC_ASSERT},

	{"MACRO", T_POP_MACRO},
	{"ENDM", T_POP_ENDM},
	{"SHIFT", T_POP_SHIFT},

	{"REPT", T_POP_REPT},
	{"FOR", T_POP_FOR},
	{"ENDR", T_POP_ENDR},
	{"BREAK", T_POP_BREAK},

	{"LOAD", T_POP_LOAD},
	{"ENDL", T_POP_ENDL},

	{"IF", T_POP_IF},
	{"ELSE", T_POP_ELSE},
	{"ELIF", T_POP_ELIF},
	{"ENDC", T_POP_ENDC},

	{"UNION", T_POP_UNION},
	{"NEXTU", T_POP_NEXTU},
	{"ENDU", T_POP_ENDU},

	{"WRAM0", T_SECT_WRAM0},
	{"VRAM", T_SECT_VRAM},
	{"ROMX", T_SECT_ROMX},
	{"ROM0", T_SECT_ROM0},
	{"HRAM", T_SECT_HRAM},
	{"WRAMX", T_SECT_WRAMX},
	{"SRAM", T_SECT_SRAM},
	{"OAM", T_SECT_OAM},

	{"RB", T_POP_RB},
	{"RW", T_POP_RW},
	// Handled before as T_Z80_RL
	// {"RL", T_POP_RL},

	{"EQU", T_POP_EQU},
	{"EQUS", T_POP_EQUS},
	{"REDEF", T_POP_REDEF},

	{"PUSHS", T_POP_PUSHS},
	{"POPS", T_POP_POPS},
	{"PUSHO", T_POP_PUSHO},
	{"POPO", T_POP_POPO},

	{"OPT", T_POP_OPT},

	{".", T_PERIOD},
};

static bool isWhitespace(int c)
{
	return c == ' ' || c == '\t';
}

LexerState *lexerState = nullptr;
LexerState *lexerStateEOL = nullptr;

static void initState(LexerState &state)
{
	state.mode = LEXER_NORMAL;
	state.atLineStart = true; // yylex() will init colNo due to this
	state.lastToken = T_EOF;

	state.ifStack.clear();

	state.capturing = false;
	state.captureBuf = nullptr;

	state.disableMacroArgs = false;
	state.disableInterpolation = false;
	state.macroArgScanDistance = 0;
	state.expandStrings = true;

	state.expansions.clear();
}

static void nextLine()
{
	lexerState->lineNo++;
	lexerState->colNo = 1;
}

uint32_t lexer_GetIFDepth()
{
	return lexerState->ifStack.size();
}

void lexer_IncIFDepth()
{
	lexerState->ifStack.push_front({ .ranIfBlock = false, .reachedElseBlock = false });
}

void lexer_DecIFDepth()
{
	if (lexerState->ifStack.empty())
		fatalerror("Found ENDC outside an IF construct\n");

	lexerState->ifStack.pop_front();
}

bool lexer_RanIFBlock()
{
	return lexerState->ifStack.front().ranIfBlock;
}

bool lexer_ReachedELSEBlock()
{
	return lexerState->ifStack.front().reachedElseBlock;
}

void lexer_RunIFBlock()
{
	lexerState->ifStack.front().ranIfBlock = true;
}

void lexer_ReachELSEBlock()
{
	lexerState->ifStack.front().reachedElseBlock = true;
}

bool lexer_OpenFile(LexerState &state, char const *path)
{
	bool isStdin = !strcmp(path, "-");
	struct stat fileInfo;

	// Give stdin a nicer file name
	if (isStdin)
		path = "<stdin>";
	if (!isStdin && stat(path, &fileInfo) != 0) {
		error("Failed to stat file \"%s\": %s\n", path, strerror(errno));
		return false;
	}
	state.path = path;
	state.isFile = true;
	state.cbuf.fd = isStdin ? STDIN_FILENO : open(path, O_RDONLY);
	if (state.cbuf.fd < 0) {
		error("Failed to open file \"%s\": %s\n", path, strerror(errno));
		return false;
	}
	state.isMmapped = false; // By default, assume it won't be mmap()ed
	if (!isStdin && fileInfo.st_size > 0) {
		// Try using `mmap` for better performance

		// Important: do NOT assign to `state.mmap.ptr` directly, to avoid a cast that may
		// alter an eventual `MAP_FAILED` value. It would also invalidate `state.cbuf.fd`,
		// being on the other side of the union.
		void *mappingAddr;

		mapFile(mappingAddr, state.cbuf.fd, state.path, fileInfo.st_size);
		if (mappingAddr == MAP_FAILED) {
			// If mmap()ing failed, try again using another method (below)
			state.isMmapped = false;
		} else {
			// IMPORTANT: the `union` mandates this is accessed before other members!
			close(state.cbuf.fd);

			state.isMmapped = true;
			state.mmap.isReferenced = false; // By default, a state isn't referenced
			state.mmap.ptr = (char *)mappingAddr;
			assert(fileInfo.st_size >= 0);
			state.mmap.size = (size_t)fileInfo.st_size;
			state.mmap.offset = 0;

			if (verbose)
				printf("File %s successfully mmap()ped\n", path);
		}
	}
	if (!state.isMmapped) {
		// Sometimes mmap() fails or isn't available, so have a fallback
		if (verbose) {
			if (isStdin)
				printf("Opening stdin\n");
			else if (fileInfo.st_size == 0)
				printf("File %s is empty\n", path);
			else
				printf("File %s opened as regular, errno reports \"%s\"\n",
				       path, strerror(errno));
		}
		state.cbuf.index = 0;
		state.cbuf.nbChars = 0;
	}

	initState(state);
	state.lineNo = 0; // Will be incremented at first line start
	return true;
}

void lexer_OpenFileView(LexerState &state, char const *path, char *buf, size_t size, uint32_t lineNo)
{
	state.path = path; // Used to report read errors in `peekInternal`
	state.isFile = false;
	state.isMmapped = true; // It's not *really* mmap()ed, but it behaves the same
	state.mmap.ptr = buf;
	state.mmap.size = size;
	state.mmap.offset = 0;

	initState(state);
	state.lineNo = lineNo; // Will be incremented at first line start
}

void lexer_RestartRept(uint32_t lineNo)
{
	lexerState->mmap.offset = 0;
	initState(*lexerState);
	lexerState->lineNo = lineNo;
}

void lexer_CleanupState(LexerState &state)
{
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

	if (!state.isMmapped)
		close(state.cbuf.fd);
	else if (state.isFile && !state.mmap.isReferenced)
		munmap(state.mmap.ptr, state.mmap.size);
}

void lexer_SetMode(enum LexerMode mode)
{
	lexerState->mode = mode;
}

void lexer_ToggleStringExpansion(bool enable)
{
	lexerState->expandStrings = enable;
}

// Functions for the actual lexer to obtain characters

static void reallocCaptureBuf()
{
	if (lexerState->captureCapacity == SIZE_MAX)
		fatalerror("Cannot grow capture buffer past %zu bytes\n", SIZE_MAX);
	else if (lexerState->captureCapacity > SIZE_MAX / 2)
		lexerState->captureCapacity = SIZE_MAX;
	else
		lexerState->captureCapacity *= 2;
	lexerState->captureBuf = (char *)realloc(lexerState->captureBuf, lexerState->captureCapacity);
	if (!lexerState->captureBuf)
		fatalerror("realloc error while resizing capture buffer: %s\n", strerror(errno));
}

static void beginExpansion(char const *str, bool owned, char const *name)
{
	size_t size = strlen(str);

	// Do not expand empty strings
	if (!size)
		return;

	if (name)
		lexer_CheckRecursionDepth();

	lexerState->expansions.push_front({
		.name = name ? strdup(name) : nullptr,
		.contents = { .unowned = str },
		.size = size,
		.offset = 0,
		.owned = owned,
	});
}

void lexer_CheckRecursionDepth()
{
	if (lexerState->expansions.size() > maxRecursionDepth + 1)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);
}

static void freeExpansion(Expansion &expansion)
{
	free(expansion.name);
	if (expansion.owned)
		free(expansion.contents.owned);
}

static bool isMacroChar(char c)
{
	return c == '@' || c == '#' || c == '<' || (c >= '0' && c <= '9');
}

// forward declarations for readBracketedMacroArgNum
static int peek();
static void shiftChar();
static uint32_t readNumber(int radix, uint32_t baseValue);
static bool startsIdentifier(int c);
static bool continuesIdentifier(int c);

static uint32_t readBracketedMacroArgNum()
{
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
		char symName[MAXSYMLEN + 1];
		size_t i = 0;

		for (; continuesIdentifier(c); c = peek()) {
			if (i < sizeof(symName))
				symName[i++] = c;
			shiftChar();
		}

		if (i == sizeof(symName)) {
			warning(WARNING_LONG_STR, "Bracketed symbol name too long, got truncated\n");
			i--;
		}
		symName[i] = '\0';

		Symbol const *sym = sym_FindScopedValidSymbol(symName);

		if (!sym) {
			error("Bracketed symbol \"%s\" does not exist\n", symName);
			num = 0;
			symbolError = true;
		} else if (!sym_IsNumeric(sym)) {
			error("Bracketed symbol \"%s\" is not numeric\n", symName);
			num = 0;
			symbolError = true;
		} else {
			num = sym_GetConstantSymValue(sym);
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

static char const *readMacroArg(char name)
{
	char const *str = nullptr;

	if (name == '@') {
		str = macro_GetUniqueIDStr();
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

static size_t readInternal(size_t bufIndex, size_t nbChars)
{
	// This buffer overflow made me lose WEEKS of my life. Never again.
	assert(bufIndex + nbChars <= LEXER_BUF_SIZE);
	ssize_t nbReadChars = read(lexerState->cbuf.fd, &lexerState->cbuf.buf[bufIndex], nbChars);

	if (nbReadChars == -1)
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path, strerror(errno));

	// `nbReadChars` cannot be negative, so it's fine to cast to `size_t`
	return (size_t)nbReadChars;
}

// We only need one character of lookahead, for macro arguments
static int peekInternal(uint8_t distance)
{
	for (Expansion &exp : lexerState->expansions) {
		// An expansion that has reached its end will have `exp->offset` == `exp->size`,
		// and `peekInternal` will continue with its parent
		assert(exp.offset <= exp.size);
		if (distance < exp.size - exp.offset)
			return exp.contents.unowned[exp.offset + distance];
		distance -= exp.size - exp.offset;
	}

	if (distance >= LEXER_BUF_SIZE)
		fatalerror("Internal lexer error: buffer has insufficient size for peeking (%"
			   PRIu8 " >= %u)\n", distance, LEXER_BUF_SIZE);

	if (lexerState->isMmapped) {
		if (lexerState->mmap.offset + distance >= lexerState->mmap.size)
			return EOF;

		return (unsigned char)lexerState->mmap.ptr[lexerState->mmap.offset + distance];
	}

	if (lexerState->cbuf.nbChars <= distance) {
		// Buffer isn't full enough, read some chars in
		size_t target = LEXER_BUF_SIZE - lexerState->cbuf.nbChars; // Aim: making the buf full

		// Compute the index we'll start writing to
		size_t writeIndex = (lexerState->cbuf.index + lexerState->cbuf.nbChars) % LEXER_BUF_SIZE;

		// If the range to fill passes over the buffer wrapping point, we need two reads
		if (writeIndex + target > LEXER_BUF_SIZE) {
			size_t nbExpectedChars = LEXER_BUF_SIZE - writeIndex;
			size_t nbReadChars = readInternal(writeIndex, nbExpectedChars);

			lexerState->cbuf.nbChars += nbReadChars;

			writeIndex += nbReadChars;
			if (writeIndex == LEXER_BUF_SIZE)
				writeIndex = 0;

			// If the read was incomplete, don't perform a second read
			target -= nbReadChars;
			if (nbReadChars < nbExpectedChars)
				target = 0;
		}
		if (target != 0)
			lexerState->cbuf.nbChars += readInternal(writeIndex, target);

		// If there aren't enough chars even after refilling, give up
		if (lexerState->cbuf.nbChars <= distance)
			return EOF;
	}

	return (unsigned char)lexerState->cbuf.buf[(lexerState->cbuf.index + distance) % LEXER_BUF_SIZE];
}

// forward declarations for peek
static void shiftChar();
static char const *readInterpolation(size_t depth);

static int peek()
{
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

static void shiftChar()
{
	if (lexerState->capturing) {
		if (lexerState->captureBuf) {
			if (lexerState->captureSize + 1 >= lexerState->captureCapacity)
				reallocCaptureBuf();
			// TODO: improve this?
			lexerState->captureBuf[lexerState->captureSize] = peek();
		}
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
		if (lexerState->isMmapped) {
			lexerState->mmap.offset++;
		} else {
			assert(lexerState->cbuf.index < LEXER_BUF_SIZE);
			lexerState->cbuf.index++;
			if (lexerState->cbuf.index == LEXER_BUF_SIZE)
				lexerState->cbuf.index = 0; // Wrap around if necessary
			assert(lexerState->cbuf.nbChars > 0);
			lexerState->cbuf.nbChars--;
		}
	}
}

static int nextChar()
{
	int c = peek();

	// If not at EOF, advance read position
	if (c != EOF)
		shiftChar();
	return c;
}

static void handleCRLF(int c)
{
	if (c == '\r' && peek() == '\n')
		shiftChar();
}

// "Services" provided by the lexer to the rest of the program

char const *lexer_GetFileName()
{
	return lexerState ? lexerState->path : nullptr;
}

uint32_t lexer_GetLineNo()
{
	return lexerState->lineNo;
}

uint32_t lexer_GetColNo()
{
	return lexerState->colNo;
}

void lexer_DumpStringExpansions()
{
	if (!lexerState)
		return;

	for (Expansion &exp : lexerState->expansions) {
		// Only register EQUS expansions, not string args
		if (exp.name)
			fprintf(stderr, "while expanding symbol \"%s\"\n", exp.name);
	}
}

// Discards a block comment
static void discardBlockComment()
{
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
				warning(WARNING_NESTED_COMMENT,
					"/* in block comment\n");
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

// Function to discard all of a line's comments

static void discardComment()
{
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

// Function to read a line continuation

static void readLineContinuation()
{
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
			error("Begun line continuation, but encountered character %s\n",
			      printChar(c));
			break;
		}
	}
}

// Function to read an anonymous label ref

static void readAnonLabelRef(char c)
{
	uint32_t n = 0;

	// We come here having already peeked at one char, so no need to do it again
	do {
		shiftChar();
		n++;
	} while (peek() == c);

	sym_WriteAnonLabelName(yylval.symName, n, c == '-');
}

// Functions to lex numbers of various radixes

static uint32_t readNumber(int radix, uint32_t baseValue)
{
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

static uint32_t readFractionalPart(uint32_t integer)
{
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
				warning(WARNING_LARGE_CONSTANT,
					"Precision of fixed-point constant is too large\n");
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

static uint32_t readBinaryNumber()
{
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

static uint32_t readHexNumber()
{
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

static uint32_t readGfxConstant()
{
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
		warning(WARNING_LARGE_CONSTANT,
			"Graphics constant is too long, only first 8 pixels considered\n");

	return bitPlaneUpper << 8 | bitPlaneLower;
}

// Functions to read identifiers & keywords

static bool startsIdentifier(int c)
{
	// Anonymous labels internally start with '!'
	return (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || c == '.' || c == '_';
}

static bool continuesIdentifier(int c)
{
	return startsIdentifier(c) || (c <= '9' && c >= '0') || c == '#' || c == '@';
}

static int readIdentifier(char firstChar)
{
	// Lex while checking for a keyword
	yylval.symName[0] = firstChar;
	int tokenType = firstChar == '.' ? T_LOCAL_ID : T_ID;
	size_t i = 1;

	// Continue reading while the char is in the symbol charset
	for (int c = peek(); continuesIdentifier(c); i++, c = peek()) {
		shiftChar();

		if (i < sizeof(yylval.symName) - 1) {
			// Write the char to the identifier's name
			yylval.symName[i] = c;

			// If the char was a dot, mark the identifier as local
			if (c == '.')
				tokenType = T_LOCAL_ID;
		}
	}

	if (i > sizeof(yylval.symName) - 1) {
		warning(WARNING_LONG_STR, "Symbol name too long, got truncated\n");
		i = sizeof(yylval.symName) - 1;
	}
	yylval.symName[i] = '\0'; // Terminate the string

	// Attempt to check for a keyword
	auto search = keywordDict.find(yylval.symName);

	return search != keywordDict.end() ? search->second : tokenType;
}

// Functions to read strings

static char const *readInterpolation(size_t depth)
{
	if (depth > maxRecursionDepth)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);

	char symName[MAXSYMLEN + 1];
	size_t i = 0;
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
			if (i == sizeof(symName)) {
				warning(WARNING_LONG_STR, "Format spec too long, got truncated\n");
				i = sizeof(symName) - 1;
			}
			symName[i] = '\0';
			for (size_t j = 0; j < i; j++)
				fmt.useCharacter(symName[j]);
			fmt.finishCharacters();
			if (!fmt.isValid())
				error("Invalid format spec '%s'\n", symName);
			i = 0; // Now that format has been set, restart at beginning of string
		} else {
			shiftChar();
			if (i < sizeof(symName)) // Allow writing an extra char to flag overflow
				symName[i++] = c;
		}
	}

	if (i == sizeof(symName)) {
		warning(WARNING_LONG_STR, "Interpolated symbol name too long, got truncated\n");
		i--;
	}
	symName[i] = '\0';

	// Don't return before `lexerState->disableInterpolation` is reset!
	lexerState->disableInterpolation = disableInterpolation;

	static char buf[MAXSTRLEN + 1];

	Symbol const *sym = sym_FindScopedValidSymbol(symName);

	if (!sym) {
		error("Interpolated symbol \"%s\" does not exist\n", symName);
	} else if (sym->type == SYM_EQUS) {
		fmt.printString(buf, sizeof(buf), sym_GetStringValue(sym));
		return buf;
	} else if (sym_IsNumeric(sym)) {
		fmt.printNumber(buf, sizeof(buf), sym_GetConstantSymValue(sym));
		return buf;
	} else {
		error("Only numerical and string symbols can be interpolated\n");
	}
	return nullptr;
}

#define append_yylval_string(c) do { \
	/* Evaluate c exactly once in case it has side effects */ \
	if (char v = (c); i < sizeof(yylval.string)) \
		yylval.string[i++] = v; \
} while (0)

static size_t appendEscapedSubstring(char const *str, size_t i)
{
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

static void readString(bool raw)
{
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
				readLineContinuation();
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

static size_t appendStringLiteral(size_t i, bool raw)
{
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
				readLineContinuation();
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
					i = appendEscapedSubstring(str, i);
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
				i = appendEscapedSubstring(str, i);
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

static int yylex_SKIP_TO_ENDC(); // forward declaration for yylex_NORMAL

static int yylex_NORMAL()
{
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
			return T_OP_NOT;

		case '@':
			yylval.symName[0] = '@';
			yylval.symName[1] = '\0';
			return T_ID;

		case '[':
			return T_LBRACK;
		case ']':
			return T_RBRACK;
		case '(':
			return T_LPAREN;
		case ')':
			return T_RPAREN;
		case ',':
			return T_COMMA;

		// Handle ambiguous 1- or 2-char tokens

		case '+': // Either += or ADD
			if (peek() == '=') {
				shiftChar();
				return T_POP_ADDEQ;
			}
			return T_OP_ADD;

		case '-': // Either -= or SUB
			if (peek() == '=') {
				shiftChar();
				return T_POP_SUBEQ;
			}
			return T_OP_SUB;

		case '*': // Either *=, MUL, or EXP
			switch (peek()) {
			case '=':
				shiftChar();
				return T_POP_MULEQ;
			case '*':
				shiftChar();
				return T_OP_EXP;
			default:
				return T_OP_MUL;
			}

		case '/': // Either /=, DIV, or a block comment
			switch (peek()) {
			case '=':
				shiftChar();
				return T_POP_DIVEQ;
			case '*':
				shiftChar();
				discardBlockComment();
				break;
			default:
				return T_OP_DIV;
			}
			break;

		case '|': // Either |=, binary OR, or logical OR
			switch (peek()) {
			case '=':
				shiftChar();
				return T_POP_OREQ;
			case '|':
				shiftChar();
				return T_OP_LOGICOR;
			default:
				return T_OP_OR;
			}

		case '^': // Either ^= or XOR
			if (peek() == '=') {
				shiftChar();
				return T_POP_XOREQ;
			}
			return T_OP_XOR;

		case '=': // Either assignment or EQ
			if (peek() == '=') {
				shiftChar();
				return T_OP_LOGICEQU;
			}
			return T_POP_EQUAL;

		case '!': // Either a NEQ or negation
			if (peek() == '=') {
				shiftChar();
				return T_OP_LOGICNE;
			}
			return T_OP_LOGICNOT;

		// Handle ambiguous 1-, 2-, or 3-char tokens

		case '<': // Either <<=, LT, LTE, or left shift
			switch (peek()) {
			case '=':
				shiftChar();
				return T_OP_LOGICLE;
			case '<':
				shiftChar();
				if (peek() == '=') {
					shiftChar();
					return T_POP_SHLEQ;
				}
				return T_OP_SHL;
			default:
				return T_OP_LOGICLT;
			}

		case '>': // Either >>=, GT, GTE, or either kind of right shift
			switch (peek()) {
			case '=':
				shiftChar();
				return T_OP_LOGICGE;
			case '>':
				shiftChar();
				switch (peek()) {
				case '=':
					shiftChar();
					return T_POP_SHREQ;
				case '>':
					shiftChar();
					return T_OP_USHR;
				default:
					return T_OP_SHR;
				}
			default:
				return T_OP_LOGICGT;
			}

		case ':': // Either :, ::, or an anonymous label ref
			c = peek();
			switch (c) {
			case ':':
				shiftChar();
				return T_DOUBLE_COLON;
			case '+':
			case '-':
				readAnonLabelRef(c);
				return T_ANON;
			default:
				return T_COLON;
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
			yylval.constValue = n;
			return T_NUMBER;
		}

		case '&': // Either &=, binary AND, logical AND, or an octal constant
			c = peek();
			if (c == '=') {
				shiftChar();
				return T_POP_ANDEQ;
			} else if (c == '&') {
				shiftChar();
				return T_OP_LOGICAND;
			} else if (c >= '0' && c <= '7') {
				yylval.constValue = readNumber(8, 0);
				return T_NUMBER;
			}
			return T_OP_AND;

		case '%': // Either %=, MOD, or a binary constant
			c = peek();
			if (c == '=') {
				shiftChar();
				return T_POP_MODEQ;
			} else if (c == binDigits[0] || c == binDigits[1]) {
				yylval.constValue = readBinaryNumber();
				return T_NUMBER;
			}
			return T_OP_MOD;

		case '$': // Hex constant
			yylval.constValue = readHexNumber();
			return T_NUMBER;

		case '`': // Gfx constant
			yylval.constValue = readGfxConstant();
			return T_NUMBER;

		// Handle strings

		case '"':
			readString(false);
			return T_STRING;

		// Handle newlines and EOF

		case '\r':
			handleCRLF(c);
			// fallthrough
		case '\n':
			return T_NEWLINE;

		case EOF:
			return T_EOF;

		// Handle line continuations

		case '\\':
			// Macro args were handled by `peek`, and character escapes do not exist
			// outside of string literals, so this must be a line continuation.
			readLineContinuation();
			break;

		// Handle raw strings... or fall through if '#' is not followed by '"'

		case '#':
			if (peek() == '"') {
				shiftChar();
				readString(true);
				return T_STRING;
			}
			// fallthrough

		// Handle identifiers... or report garbage characters

		default:
			if (startsIdentifier(c)) {
				int tokenType = readIdentifier(c);

				// An ELIF after a taken IF needs to not evaluate its condition
				if (tokenType == T_POP_ELIF && lexerState->lastToken == T_NEWLINE
				 && lexer_GetIFDepth() > 0 && lexer_RanIFBlock()
				 && !lexer_ReachedELSEBlock())
					return yylex_SKIP_TO_ENDC();

				// If a keyword, don't try to expand
				if (tokenType != T_ID && tokenType != T_LOCAL_ID)
					return tokenType;

				// Local symbols cannot be string expansions
				if (tokenType == T_ID && lexerState->expandStrings) {
					// Attempt string expansion
					Symbol const *sym = sym_FindExactSymbol(yylval.symName);

					if (sym && sym->type == SYM_EQUS) {
						char const *s = sym_GetStringValue(sym);

						assert(s);
						if (s[0])
							beginExpansion(s, false, sym->name);
						continue; // Restart, reading from the new buffer
					}
				}

				if (tokenType == T_ID && (lexerState->atLineStart || peek() == ':'))
					return T_LABEL;

				return tokenType;
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

static int yylex_RAW()
{
	// This is essentially a modified `appendStringLiteral`
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
			readLineContinuation();
		} else {
			break;
		}
	}

	for (;;) {
		c = peek();

		switch (c) {
		case '"': // String literals inside macro args
			shiftChar();
			i = appendStringLiteral(i, false);
			break;

		case '#': // Raw string literals inside macro args
			append_yylval_string(c);
			shiftChar();
			if (peek() == '"') {
				shiftChar();
				i = appendStringLiteral(i, true);
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
				readLineContinuation();
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

	// Returning T_COMMAs to the parser would mean that two consecutive commas
	// (i.e. an empty argument) need to return two different tokens (T_STRING
	// then T_COMMA) without advancing the read. To avoid this, commas in raw
	// mode end the current macro argument but are not tokenized themselves.
	if (c == ',') {
		shiftChar();
		return T_STRING;
	}

	// The last argument may end in a trailing comma, newline, or EOF.
	// To allow trailing commas, raw mode will continue after the last
	// argument, immediately lexing the newline or EOF again (i.e. with
	// an empty raw string before it). This will not be treated as a
	// macro argument. To pass an empty last argument, use a second
	// trailing comma.
	if (i > 0)
		return T_STRING;
	lexer_SetMode(LEXER_NORMAL);

	if (c == '\r' || c == '\n') {
		shiftChar();
		handleCRLF(c);
		return T_NEWLINE;
	}

	return T_EOF;
}

#undef append_yylval_string

// This function uses the fact that `if`, etc. constructs are only valid when
// there's nothing before them on their lines. This enables filtering
// "meaningful" (= at line start) vs. "meaningless" (everything else) tokens.
// It's especially important due to macro args not being handled in this
// state, and lexing them in "normal" mode potentially producing such tokens.
static int skipIfBlock(bool toEndc)
{
	lexer_SetMode(LEXER_NORMAL);
	uint32_t startingDepth = lexer_GetIFDepth();
	int token;
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
				switch (token) {
				case T_POP_IF:
					lexer_IncIFDepth();
					break;

				case T_POP_ELIF:
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELIF after an ELSE block\n");
					goto maybeFinish;

				case T_POP_ELSE:
					if (lexer_ReachedELSEBlock())
						fatalerror("Found ELSE after an ELSE block\n");
					lexer_ReachELSEBlock();
					// fallthrough
				maybeFinish:
					if (toEndc) // Ignore ELIF and ELSE, go to ENDC
						break;
					// fallthrough
				case T_POP_ENDC:
					if (lexer_GetIFDepth() == startingDepth)
						goto finish;
					if (token == T_POP_ENDC)
						lexer_DecIFDepth();
				}
			}
			atLineStart = false;
		}

		// Read chars until EOL
		do {
			int c = nextChar();

			if (c == EOF) {
				token = T_EOF;
				goto finish;
			} else if (c == '\\') {
				// Unconditionally skip the next char, including line conts
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

static int yylex_SKIP_TO_ELIF()
{
	return skipIfBlock(false);
}

static int yylex_SKIP_TO_ENDC()
{
	return skipIfBlock(true);
}

static int yylex_SKIP_TO_ENDR()
{
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
				switch (readIdentifier(c)) {
				case T_POP_FOR:
				case T_POP_REPT:
					depth++;
					break;

				case T_POP_ENDR:
					depth--;
					if (!depth)
						goto finish;
					break;

				case T_POP_IF:
					lexer_IncIFDepth();
					break;

				case T_POP_ENDC:
					lexer_DecIFDepth();
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
				// Unconditionally skip the next char, including line conts
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
	return T_EOF;
}

int yylex()
{
	if (lexerState->atLineStart && lexerStateEOL) {
		lexer_SetState(lexerStateEOL);
		lexerStateEOL = nullptr;
	}
	// `lexer_SetState` updates `lexerState`, so check for EOF after it
	if (lexerState->lastToken == T_EOB && yywrap())
		return T_EOF;
	// Newlines read within an expansion should not increase the line count
	if (lexerState->atLineStart && lexerState->expansions.empty())
		nextLine();

	static int (* const lexerModeFuncs[NB_LEXER_MODES])() = {
		yylex_NORMAL,
		yylex_RAW,
		yylex_SKIP_TO_ELIF,
		yylex_SKIP_TO_ENDC,
		yylex_SKIP_TO_ENDR,
	};
	int token = lexerModeFuncs[lexerState->mode]();

	// Captures end at their buffer's boundary no matter what
	if (token == T_EOF && !lexerState->capturing)
		token = T_EOB;
	lexerState->lastToken = token;
	lexerState->atLineStart = token == T_NEWLINE || token == T_EOB;

	return token;
}

static void startCapture(CaptureBody *capture)
{
	assert(!lexerState->capturing);
	lexerState->capturing = true;
	lexerState->captureSize = 0;
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	capture->lineNo = lexer_GetLineNo();

	if (lexerState->isMmapped && lexerState->expansions.empty()) {
		capture->body = &lexerState->mmap.ptr[lexerState->mmap.offset];
	} else {
		lexerState->captureCapacity = 128; // The initial size will be twice that
		assert(lexerState->captureBuf == nullptr);
		reallocCaptureBuf();
		capture->body = nullptr; // Indicate to retrieve the capture buffer when done capturing
	}
}

static void endCapture(CaptureBody *capture)
{
	// This being `nullptr` means we're capturing from the capture buf, which is `realloc`ed
	// during the whole capture process, and so MUST be retrieved at the end
	if (!capture->body)
		capture->body = lexerState->captureBuf;
	capture->size = lexerState->captureSize;

	lexerState->capturing = false;
	lexerState->captureBuf = nullptr;
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

bool lexer_CaptureRept(CaptureBody *capture)
{
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
			switch (readIdentifier(c)) {
			case T_POP_REPT:
			case T_POP_FOR:
				depth++;
				// Ignore the rest of that line
				break;

			case T_POP_ENDR:
				if (!depth) {
					// The final ENDR has been captured, but we don't want it!
					// We know we have read exactly "ENDR", not e.g. an EQUS
					lexerState->captureSize -= strlen("ENDR");
					goto finish;
				}
				depth--;
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

bool lexer_CaptureMacroBody(CaptureBody *capture)
{
	startCapture(capture);

	// If the file is `mmap`ed, we need not to unmap it to keep access to the macro
	if (lexerState->isMmapped)
		lexerState->mmap.isReferenced = true;

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
			switch (readIdentifier(c)) {
			case T_POP_ENDM:
				// The ENDM has been captured, but we don't want it!
				// We know we have read exactly "ENDM", not e.g. an EQUS
				lexerState->captureSize -= strlen("ENDM");
				goto finish;
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
