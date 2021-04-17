/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "extern/utf8decoder.h"
#include "platform.h" /* For `ssize_t` */

#include "asm/lexer.h"
#include "asm/format.h"
#include "asm/fstack.h"
#include "asm/macro.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/symbol.h"
#include "asm/util.h"
#include "asm/warning.h"
/* Include this last so it gets all type & constant definitions */
#include "parser.h" /* For token definitions, generated from parser.y */

#ifdef LEXER_DEBUG
  #define dbgPrint(...) fprintf(stderr, "[lexer] " __VA_ARGS__)
#else
  #define dbgPrint(...)
#endif

/* Neither MSVC nor MinGW provide `mmap` */
#if defined(_MSC_VER) || defined(__MINGW32__)
# define WIN32_LEAN_AND_MEAN // include less from windows.h
# include <windows.h> // target architecture
# include <fileapi.h> // CreateFileA
# include <winbase.h> // CreateFileMappingA
# include <memoryapi.h> // MapViewOfFile
# include <handleapi.h> // CloseHandle
# define MAP_FAILED NULL
# define mapFile(ptr, fd, path, size) do { \
	(ptr) = MAP_FAILED; \
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, \
				  FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_RANDOM_ACCESS, NULL); \
	HANDLE mappingObj; \
	\
	if (file == INVALID_HANDLE_VALUE) \
		break; \
	mappingObj  = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL); \
	if (mappingObj != INVALID_HANDLE_VALUE) \
		(ptr) = MapViewOfFile(mappingObj, FILE_MAP_READ, 0, 0, 0); \
	CloseHandle(mappingObj); \
	CloseHandle(file); \
} while (0)
# define munmap(ptr, size)  UnmapViewOfFile((ptr))

#else /* defined(_MSC_VER) || defined(__MINGW32__) */

# include <sys/mman.h>
# define mapFile(ptr, fd, path, size) do { \
	(ptr) = mmap(NULL, (size), PROT_READ, MAP_PRIVATE, (fd), 0); \
	\
	if ((ptr) == MAP_FAILED && errno == ENOTSUP) { \
		/*
		 * The implementation may not support MAP_PRIVATE; try again with MAP_SHARED
		 * instead, offering, I believe, weaker guarantees about external modifications to
		 * the file while reading it. That's still better than not opening it at all, though
		 */ \
		if (verbose) \
			printf("mmap(%s, MAP_PRIVATE) failed, retrying with MAP_SHARED\n", path); \
		(ptr) = mmap(NULL, (size), PROT_READ, MAP_SHARED, (fd), 0); \
	} \
} while (0)
#endif /* !( defined(_MSC_VER) || defined(__MINGW32__) ) */

/*
 * Identifiers that are also keywords are listed here. This ONLY applies to ones
 * that would normally be matched as identifiers! Check out `yylex_NORMAL` to
 * see how this is used.
 * Tokens / keywords not handled here are handled in `yylex_NORMAL`'s switch.
 */
static struct KeywordMapping {
	char const *name;
	int token;
} const keywords[] = {
	/*
	 * CAUTION when editing this: adding keywords will probably require extra nodes in the
	 * `keywordDict` array. If you forget to, you will probably trip up an assertion, anyways.
	 * Also, all entries in this array must be in uppercase for the dict to build correctly.
	 */
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
	{"SET", T_POP_SET},
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
	/* Handled after as T_TOKEN_C */
	/* { "C", T_CC_C }, */

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

	{"ROUND", T_OP_ROUND},
	{"CEIL", T_OP_CEIL},
	{"FLOOR", T_OP_FLOOR},
	{"DIV", T_OP_FDIV},
	{"MUL", T_OP_FMUL},
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

	{"INCLUDE", T_POP_INCLUDE},
	{"PRINT", T_POP_PRINT},
	{"PRINTLN", T_POP_PRINTLN},
	{"PRINTT", T_POP_PRINTT},
	{"PRINTI", T_POP_PRINTI},
	{"PRINTV", T_POP_PRINTV},
	{"PRINTF", T_POP_PRINTF},
	{"EXPORT", T_POP_EXPORT},
	{"DS", T_POP_DS},
	{"DB", T_POP_DB},
	{"DW", T_POP_DW},
	{"DL", T_POP_DL},
	{"SECTION", T_POP_SECTION},
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
	/* Handled before as T_Z80_RL */
	/* {"RL", T_POP_RL}, */

	{"EQU", T_POP_EQU},
	{"EQUS", T_POP_EQUS},
	{"REDEF", T_POP_REDEF},
	/* Handled before as T_Z80_SET */
	/* {"SET", T_POP_SET}, */

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

#define LEXER_BUF_SIZE 42 /* TODO: determine a sane value for this */
/* This caps the size of buffer reads, and according to POSIX, passing more than SSIZE_MAX is UB */
static_assert(LEXER_BUF_SIZE <= SSIZE_MAX, "Lexer buffer size is too large");

struct Expansion {
	struct Expansion *parent;
	char *name;
	union {
		char const *unowned;
		char *owned;
	} contents;
	size_t size; /* Length of the contents */
	size_t offset; /* Cursor into the contents */
	bool owned; /* Whether or not to free contents when this expansion is freed */
};

struct IfStack {
	struct IfStack *next;
	bool ranIfBlock; /* Whether an IF/ELIF/ELSE block ran already */
	bool reachedElseBlock; /* Whether an ELSE block ran already */
};

struct LexerState {
	char const *path;

	/* mmap()-dependent IO state */
	bool isMmapped;
	union {
		struct { /* If mmap()ed */
			char *ptr; /* Technically `const` during the lexer's execution */
			size_t size;
			size_t offset;
			bool isReferenced; /* If a macro in this file requires not unmapping it */
		};
		struct { /* Otherwise */
			int fd;
			size_t index; /* Read index into the buffer */
			char buf[LEXER_BUF_SIZE]; /* Circular buffer */
			size_t nbChars; /* Number of "fresh" chars in the buffer */
		};
	};

	/* Common state */
	bool isFile;

	enum LexerMode mode;
	bool atLineStart;
	uint32_t lineNo;
	uint32_t colNo;
	int lastToken;
	int nextToken;

	struct IfStack *ifStack;

	bool capturing; /* Whether the text being lexed should be captured */
	size_t captureSize; /* Amount of text captured */
	char *captureBuf; /* Buffer to send the captured text to if non-NULL */
	size_t captureCapacity; /* Size of the buffer above */

	bool disableMacroArgs;
	bool disableInterpolation;
	size_t macroArgScanDistance; /* Max distance already scanned for macro args */
	bool expandStrings;
	struct Expansion *expansions; /* Points to the innermost current expansion */
};

struct LexerState *lexerState = NULL;
struct LexerState *lexerStateEOL = NULL;

static void initState(struct LexerState *state)
{
	state->mode = LEXER_NORMAL;
	state->atLineStart = true; /* yylex() will init colNo due to this */
	state->lastToken = T_EOF;
	state->nextToken = 0;

	state->ifStack = NULL;

	state->capturing = false;
	state->captureBuf = NULL;

	state->disableMacroArgs = false;
	state->disableInterpolation = false;
	state->macroArgScanDistance = 0;
	state->expandStrings = true;
	state->expansions = NULL;
}

static void nextLine(void)
{
	lexerState->lineNo++;
	lexerState->colNo = 1;
}

uint32_t lexer_GetIFDepth(void)
{
	uint32_t depth = 0;

	for (struct IfStack *stack = lexerState->ifStack; stack != NULL; stack = stack->next)
		depth++;

	return depth;
}

void lexer_IncIFDepth(void)
{
	struct IfStack *new = malloc(sizeof(*new));

	if (!new)
		fatalerror("Unable to allocate new IF depth: %s\n", strerror(errno));

	new->ranIfBlock = false;
	new->reachedElseBlock = false;
	new->next = lexerState->ifStack;

	lexerState->ifStack = new;
}

void lexer_DecIFDepth(void)
{
	if (!lexerState->ifStack)
		fatalerror("Found ENDC outside an IF construct\n");

	struct IfStack *top = lexerState->ifStack->next;

	free(lexerState->ifStack);

	lexerState->ifStack = top;
}

bool lexer_RanIFBlock(void)
{
	return lexerState->ifStack->ranIfBlock;
}

bool lexer_ReachedELSEBlock(void)
{
	return lexerState->ifStack->reachedElseBlock;
}

void lexer_RunIFBlock(void)
{
	lexerState->ifStack->ranIfBlock = true;
}

void lexer_ReachELSEBlock(void)
{
	lexerState->ifStack->reachedElseBlock = true;
}

struct LexerState *lexer_OpenFile(char const *path)
{
	dbgPrint("Opening file \"%s\"\n", path);

	bool isStdin = !strcmp(path, "-");
	struct LexerState *state = malloc(sizeof(*state));
	struct stat fileInfo;

	/* Give stdin a nicer file name */
	if (isStdin)
		path = "<stdin>";
	if (!state) {
		error("Failed to allocate memory for lexer state: %s\n", strerror(errno));
		return NULL;
	}
	if (!isStdin && stat(path, &fileInfo) != 0) {
		error("Failed to stat file \"%s\": %s\n", path, strerror(errno));
		free(state);
		return NULL;
	}
	state->path = path;
	state->isFile = true;
	state->fd = isStdin ? STDIN_FILENO : open(path, O_RDONLY);
	if (state->fd < 0) {
		error("Failed to open file \"%s\": %s\n", path, strerror(errno));
		free(state);
		return NULL;
	}
	state->isMmapped = false; /* By default, assume it won't be mmap()ed */
	if (!isStdin && fileInfo.st_size > 0) {
		/* Try using `mmap` for better performance */

		/*
		 * Important: do NOT assign to `state->ptr` directly, to avoid a cast that may
		 * alter an eventual `MAP_FAILED` value. It would also invalidate `state->fd`,
		 * being on the other side of the union.
		 */
		void *mappingAddr;

		mapFile(mappingAddr, state->fd, state->path, fileInfo.st_size);
		if (mappingAddr == MAP_FAILED) {
			/* If mmap()ing failed, try again using another method (below) */
			state->isMmapped = false;
		} else {
			/* IMPORTANT: the `union` mandates this is accessed before other members! */
			close(state->fd);

			state->isMmapped = true;
			state->isReferenced = false; // By default, a state isn't referenced
			state->ptr = mappingAddr;
			assert(fileInfo.st_size >= 0);
			state->size = (size_t)fileInfo.st_size;
			state->offset = 0;

			if (verbose)
				printf("File %s successfully mmap()ped\n", path);
		}
	}
	if (!state->isMmapped) {
		/* Sometimes mmap() fails or isn't available, so have a fallback */
		if (verbose) {
			if (isStdin)
				printf("Opening stdin\n");
			else if (fileInfo.st_size == 0)
				printf("File %s is empty\n", path);
			else
				printf("File %s opened as regular, errno reports \"%s\"\n",
				       path, strerror(errno));
		}
		state->index = 0;
		state->nbChars = 0;
	}

	initState(state);
	state->lineNo = 0; /* Will be incremented at first line start */
	return state;
}

struct LexerState *lexer_OpenFileView(char *buf, size_t size, uint32_t lineNo)
{
	dbgPrint("Opening view on buffer \"%.*s\"[...]\n", size < 16 ? (int)size : 16, buf);

	struct LexerState *state = malloc(sizeof(*state));

	if (!state) {
		error("Failed to allocate memory for lexer state: %s\n", strerror(errno));
		return NULL;
	}
	// TODO: init `path`

	state->isFile = false;
	state->isMmapped = true; /* It's not *really* mmap()ed, but it behaves the same */
	state->ptr = buf;
	state->size = size;
	state->offset = 0;

	initState(state);
	state->lineNo = lineNo; /* Will be incremented at first line start */
	return state;
}

void lexer_RestartRept(uint32_t lineNo)
{
	dbgPrint("Restarting REPT/FOR\n");
	lexerState->offset = 0;
	initState(lexerState);
	lexerState->lineNo = lineNo;
}

void lexer_DeleteState(struct LexerState *state)
{
	// A big chunk of the lexer state soundness is the file stack ("fstack").
	// Each context in the fstack has its own *unique* lexer state; thus, we always guarantee
	// that lexer states lifetimes are always properly managed, since they're handled solely
	// by the fstack... with *one* exception.
	// Assume a context is pushed on top of the fstack, and the corresponding lexer state gets
	// scheduled at EOF; `lexerStateAtEOL` thus becomes a (weak) ref to that lexer state...
	// It has been possible, due to a bug, that the corresponding fstack context gets popped
	// before EOL, deleting the associated state... but it would still be switched to at EOL.
	// This assertion checks that this doesn't happen again.
	// It could be argued that deleting a state that's scheduled for EOF could simply clear
	// `lexerStateEOL`, but there's currently no situation in which this should happen.
	assert(state != lexerStateEOL);

	if (!state->isMmapped)
		close(state->fd);
	else if (state->isFile && !state->isReferenced)
		munmap(state->ptr, state->size);
	free(state);
}

struct KeywordDictNode {
	/*
	 * The identifier charset is (currently) 44 characters big. By storing entries for the
	 * entire printable ASCII charset, minus lower-case due to case-insensitivity,
	 * we only waste (0x60 - 0x20) - 70 = 20 entries per node, which should be acceptable.
	 * In turn, this allows greatly simplifying checking an index into this array,
	 * which should help speed up the lexer.
	 */
	uint16_t children[0x60 - ' '];
	struct KeywordMapping const *keyword;
/* Since the keyword structure is invariant, the min number of nodes is known at compile time */
} keywordDict[351] = {0}; /* Make sure to keep this correct when adding keywords! */

/* Convert a char into its index into the dict */
static uint8_t dictIndex(char c)
{
	/* Translate uppercase to lowercase (roughly) */
	if (c > 0x60)
		c = c - ('a' - 'A');
	return c - ' ';
}

void lexer_Init(void)
{
	/*
	 * Build the dictionary of keywords. This could be done at compile time instead, however:
	 *  - Doing so manually is a task nobody wants to undertake
	 *  - It would be massively hard to read
	 *  - Doing it within CC or CPP would be quite non-trivial
	 *  - Doing it externally would require some extra work to use only POSIX tools
	 *  - The startup overhead isn't much compared to the program's
	 */
	uint16_t usedNodes = 1;

	for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
		uint16_t nodeID = 0;

		/* Walk the dictionary, creating intermediate nodes for the keyword */
		for (char const *ptr = keywords[i].name; *ptr; ptr++) {
			/* We should be able to assume all entries are well-formed */
			if (keywordDict[nodeID].children[*ptr - ' '] == 0) {
				/*
				 * If this gets tripped up, set the size of keywordDict to
				 * something high, compile with `-DPRINT_NODE_COUNT` (see below),
				 * and set the size to that.
				 */
				assert(usedNodes < sizeof(keywordDict) / sizeof(*keywordDict));

				/* There is no node at that location, grab one from the pool */
				keywordDict[nodeID].children[*ptr - ' '] = usedNodes;
				usedNodes++;
			}
			nodeID = keywordDict[nodeID].children[*ptr - ' '];
		}

		/* This assumes that no two keywords have the same name */
		keywordDict[nodeID].keyword = &keywords[i];
	}

#ifdef PRINT_NODE_COUNT /* For the maintainer to check how many nodes are needed */
	printf("Lexer keyword dictionary: %zu keywords in %u nodes (pool size %zu)\n",
	       sizeof(keywords) / sizeof(*keywords), usedNodes,
	       sizeof(keywordDict) / sizeof(*keywordDict));
#endif
}

void lexer_SetMode(enum LexerMode mode)
{
	lexerState->mode = mode;
}

void lexer_ToggleStringExpansion(bool enable)
{
	lexerState->expandStrings = enable;
}

/* Functions for the actual lexer to obtain characters */

static void reallocCaptureBuf(void)
{
	if (lexerState->captureCapacity == SIZE_MAX)
		fatalerror("Cannot grow capture buffer past %zu bytes\n", SIZE_MAX);
	else if (lexerState->captureCapacity > SIZE_MAX / 2)
		lexerState->captureCapacity = SIZE_MAX;
	else
		lexerState->captureCapacity *= 2;
	lexerState->captureBuf = realloc(lexerState->captureBuf, lexerState->captureCapacity);
	if (!lexerState->captureBuf)
		fatalerror("realloc error while resizing capture buffer: %s\n", strerror(errno));
}

static void beginExpansion(char const *str, bool owned, char const *name)
{
	size_t size = strlen(str);

	/* Do not expand empty strings */
	if (!size)
		return;

	if (name) {
		unsigned int depth = 0;

		for (struct Expansion *exp = lexerState->expansions; exp; exp = exp->parent) {
			if (depth++ >= nMaxRecursionDepth)
				fatalerror("Recursion limit (%zu) exceeded\n", nMaxRecursionDepth);
		}
	}

	struct Expansion *new = malloc(sizeof(*new));

	if (!new)
		fatalerror("Unable to allocate new expansion: %s\n", strerror(errno));

	new->parent = lexerState->expansions;
	new->name = name ? strdup(name) : NULL;
	new->contents.unowned = str;
	new->size = size;
	new->offset = 0;
	new->owned = owned;

	lexerState->expansions = new;
}

static void freeExpansion(struct Expansion *expansion)
{
	free(expansion->name);
	if (expansion->owned)
		free(expansion->contents.owned);
	free(expansion);
}

static bool isMacroChar(char c)
{
	return c == '@' || c == '#' || (c >= '0' && c <= '9');
}

static char const *readMacroArg(char name)
{
	char const *str;

	if (name == '@')
		str = macro_GetUniqueIDStr();
	else if (name == '#')
		str = macro_GetAllArgs();
	else if (name == '0')
		fatalerror("Invalid macro argument '\\0'\n");
	else
		str = macro_GetArg(name - '0');
	if (!str)
		fatalerror("Macro argument '\\%c' not defined\n", name);

	return str;
}

/* We only need one character of lookahead, for macro arguments */
static int peekInternal(uint8_t distance)
{
	for (struct Expansion *exp = lexerState->expansions; exp; exp = exp->parent) {
		/*
		 * An expansion that has reached its end will have `exp->offset` == `exp->size`,
		 * and `peekInternal` will continue with its parent
		 */
		assert(exp->offset <= exp->size);
		if (distance < exp->size - exp->offset)
			return exp->contents.unowned[exp->offset + distance];
		distance -= exp->size - exp->offset;
	}

	if (distance >= LEXER_BUF_SIZE)
		fatalerror("Internal lexer error: buffer has insufficient size for peeking (%"
			   PRIu8 " >= %u)\n", distance, LEXER_BUF_SIZE);

	if (lexerState->isMmapped) {
		if (lexerState->offset + distance >= lexerState->size)
			return EOF;

		return (unsigned char)lexerState->ptr[lexerState->offset + distance];
	}

	if (lexerState->nbChars <= distance) {
		/* Buffer isn't full enough, read some chars in */
		size_t target = LEXER_BUF_SIZE - lexerState->nbChars; /* Aim: making the buf full */

		/* Compute the index we'll start writing to */
		size_t writeIndex = (lexerState->index + lexerState->nbChars) % LEXER_BUF_SIZE;
		ssize_t nbCharsRead = 0, totalCharsRead = 0;

#define readChars(size) do { \
	/* This buffer overflow made me lose WEEKS of my life. Never again. */ \
	assert(writeIndex + (size) <= LEXER_BUF_SIZE); \
	nbCharsRead = read(lexerState->fd, &lexerState->buf[writeIndex], (size)); \
	if (nbCharsRead == -1) \
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path, strerror(errno)); \
	totalCharsRead += nbCharsRead; \
	writeIndex += nbCharsRead; \
	if (writeIndex == LEXER_BUF_SIZE) \
		writeIndex = 0; \
	target -= nbCharsRead; \
} while (0)

		/* If the range to fill passes over the buffer wrapping point, we need two reads */
		if (writeIndex + target > LEXER_BUF_SIZE) {
			size_t nbExpectedChars = LEXER_BUF_SIZE - writeIndex;

			readChars(nbExpectedChars);
			// If the read was incomplete, don't perform a second read
			// `nbCharsRead` cannot be negative, so it's fine to cast to `size_t`
			if ((size_t)nbCharsRead < nbExpectedChars)
				target = 0;
		}
		if (target != 0)
			readChars(target);

#undef readChars

		lexerState->nbChars += totalCharsRead;

		/* If there aren't enough chars even after refilling, give up */
		if (lexerState->nbChars <= distance)
			return EOF;
	}
	return (unsigned char)lexerState->buf[(lexerState->index + distance) % LEXER_BUF_SIZE];
}

/* forward declarations for peek */
static void shiftChar(void);
static char const *readInterpolation(unsigned int depth);

static int peek(void)
{
	int c;

restart:
	c = peekInternal(0);

	if (lexerState->macroArgScanDistance > 0)
		return c;

	lexerState->macroArgScanDistance++; /* Do not consider again */

	if (c == '\\' && !lexerState->disableMacroArgs) {
		/* If character is a backslash, check for a macro arg */
		lexerState->macroArgScanDistance++;
		c = peekInternal(1);
		if (isMacroChar(c)) {
			shiftChar();
			shiftChar();
			char const *str = readMacroArg(c);

			/*
			 * If the macro arg is an empty string, it cannot be
			 * expanded, so skip it and keep peeking.
			 */
			if (!str[0])
				goto restart;

			beginExpansion(str, c == '#', NULL);

			/*
			 * Assuming macro args can't be recursive (I'll be damned if a way
			 * is found...), then we mark the entire macro arg as scanned.
			 */
			lexerState->macroArgScanDistance += strlen(str);

			c = str[0];
		} else {
			c = '\\';
		}
	} else if (c == '{' && !lexerState->disableInterpolation) {
		/* If character is an open brace, do symbol interpolation */
		shiftChar();
		char const *ptr = readInterpolation(0);

		if (ptr) {
			beginExpansion(ptr, false, ptr);
			goto restart;
		}
	}

	return c;
}

static void shiftChar(void)
{
	if (lexerState->capturing) {
		if (lexerState->captureBuf) {
			if (lexerState->captureSize + 1 >= lexerState->captureCapacity)
				reallocCaptureBuf();
			/* TODO: improve this? */
			lexerState->captureBuf[lexerState->captureSize] = peek();
		}
		lexerState->captureSize++;
	}

	lexerState->macroArgScanDistance--;

restart:
	if (lexerState->expansions) {
		/* Advance within the current expansion */
		assert(lexerState->expansions->offset <= lexerState->expansions->size);
		lexerState->expansions->offset++;
		if (lexerState->expansions->offset > lexerState->expansions->size) {
			/*
			 * When advancing would go past an expansion's end, free it,
			 * move up to its parent, and try again to advance
			 */
			struct Expansion *exp = lexerState->expansions;

			lexerState->expansions = lexerState->expansions->parent;
			freeExpansion(exp);
			goto restart;
		}
	} else {
		/* Advance within the file contents */
		lexerState->colNo++;
		if (lexerState->isMmapped) {
			lexerState->offset++;
		} else {
			assert(lexerState->index < LEXER_BUF_SIZE);
			lexerState->index++;
			if (lexerState->index == LEXER_BUF_SIZE)
				lexerState->index = 0; /* Wrap around if necessary */
			assert(lexerState->nbChars > 0);
			lexerState->nbChars--;
		}
	}
}

static int nextChar(void)
{
	int c = peek();

	/* If not at EOF, advance read position */
	if (c != EOF)
		shiftChar();
	return c;
}

static void handleCRLF(int c)
{
	if (c == '\r' && peek() == '\n')
		shiftChar();
}

/* "Services" provided by the lexer to the rest of the program */

char const *lexer_GetFileName(void)
{
	return lexerState ? lexerState->path : NULL;
}

uint32_t lexer_GetLineNo(void)
{
	return lexerState->lineNo;
}

uint32_t lexer_GetColNo(void)
{
	return lexerState->colNo;
}

void lexer_DumpStringExpansions(void)
{
	if (!lexerState)
		return;

	for (struct Expansion *exp = lexerState->expansions; exp; exp = exp->parent) {
		/* Only register EQUS expansions, not string args */
		if (exp->name)
			fprintf(stderr, "while expanding symbol \"%s\"\n", exp->name);
	}
}

/* Discards an block comment */
static void discardBlockComment(void)
{
	dbgPrint("Discarding block comment\n");
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	for (;;) {
		int c = nextChar();

		switch (c) {
		case EOF:
			error("Unterminated block comment\n");
			goto finish;
		case '\r':
			/* Handle CRLF before nextLine() since shiftChar updates colNo */
			handleCRLF(c);
			/* fallthrough */
		case '\n':
			if (!lexerState->expansions)
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
			/* fallthrough */
		default:
			continue;
		}
	}
finish:
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

/* Function to discard all of a line's comments */

static void discardComment(void)
{
	dbgPrint("Discarding comment\n");
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	for (;;) {
		int c = peek();

		if (c == EOF || c == '\r' || c == '\n')
			break;
		shiftChar();
	}
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

/* Function to read a line continuation */

static void readLineContinuation(void)
{
	dbgPrint("Beginning line continuation\n");
	for (;;) {
		int c = peek();

		if (isWhitespace(c)) {
			shiftChar();
		} else if (c == '\r' || c == '\n') {
			shiftChar();
			/* Handle CRLF before nextLine() since shiftChar updates colNo */
			handleCRLF(c);
			if (!lexerState->expansions)
				nextLine();
			return;
		} else if (c == ';') {
			discardComment();
		} else {
			error("Begun line continuation, but encountered character %s\n",
			      printChar(c));
			return;
		}
	}
}

/* Function to read an anonymous label ref */

static void readAnonLabelRef(char c)
{
	uint32_t n = 0;

	// We come here having already peeked at one char, so no need to do it again
	do {
		shiftChar();
		n++;
	} while (peek() == c);

	sym_WriteAnonLabelName(yylval.tzSym, n, c == '-');
}

/* Functions to lex numbers of various radixes */

static void readNumber(int radix, int32_t baseValue)
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

	yylval.nConstValue = value;
}

static void readFractionalPart(void)
{
	uint32_t value = 0, divisor = 1;

	dbgPrint("Reading fractional part\n");
	for (;; shiftChar()) {
		int c = peek();

		if (c == '_')
			continue;
		else if (c < '0' || c > '9')
			break;
		if (divisor > (UINT32_MAX - (c - '0')) / 10) {
			warning(WARNING_LARGE_CONSTANT,
				"Precision of fixed-point constant is too large\n");
			/* Discard any additional digits */
			shiftChar();
			while (c = peek(), (c >= '0' && c <= '9') || c == '_')
				shiftChar();
			break;
		}
		value = value * 10 + (c - '0');
		divisor *= 10;
	}

	if (yylval.nConstValue > INT16_MAX || yylval.nConstValue < INT16_MIN)
		warning(WARNING_LARGE_CONSTANT, "Magnitude of fixed-point constant is too large\n");

	/* Cast to unsigned avoids UB if shifting discards bits */
	yylval.nConstValue = (uint32_t)yylval.nConstValue << 16;
	/* Cast to unsigned avoids undefined overflow behavior */
	uint16_t fractional = (uint16_t)round(value * 65536.0 / divisor);

	yylval.nConstValue |= fractional * (yylval.nConstValue >= 0 ? 1 : -1);
}

char binDigits[2];

static void readBinaryNumber(void)
{
	uint32_t value = 0;

	dbgPrint("Reading binary number with digits [%c,%c]\n", binDigits[0], binDigits[1]);
	for (;; shiftChar()) {
		int c = peek();
		int bit;

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

	yylval.nConstValue = value;
}

static void readHexNumber(void)
{
	uint32_t value = 0;
	bool empty = true;

	dbgPrint("Reading hex number\n");
	for (;; shiftChar()) {
		int c = peek();

		if (c >= 'a' && c <= 'f') /* Convert letters to right after digits */
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

	yylval.nConstValue = value;
}

char gfxDigits[4];

static void readGfxConstant(void)
{
	uint32_t bp0 = 0, bp1 = 0;
	uint8_t width = 0;

	dbgPrint("Reading gfx constant with digits [%c,%c,%c,%c]\n",
		 gfxDigits[0], gfxDigits[1], gfxDigits[2], gfxDigits[3]);
	for (;;) {
		int c = peek();
		uint32_t pixel;

		if (c == gfxDigits[0])
			pixel = 0;
		else if (c == gfxDigits[1])
			pixel = 1;
		else if (c == gfxDigits[2])
			pixel = 2;
		else if (c == gfxDigits[3])
			pixel = 3;
		else
			break;

		if (width < 8) {
			bp0 = bp0 << 1 | (pixel & 1);
			bp1 = bp1 << 1 | (pixel >> 1);
		}
		if (width < 9)
			width++;
		shiftChar();
	}

	if (width == 0)
		error("Invalid graphics constant, no digits after '`'\n");
	else if (width == 9)
		warning(WARNING_LARGE_CONSTANT,
			"Graphics constant is too long, only 8 first pixels considered\n");

	yylval.nConstValue = bp1 << 8 | bp0;
}

/* Functions to read identifiers & keywords */

static bool startsIdentifier(int c)
{
	return (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || c == '.' || c == '_';
}

static bool continuesIdentifier(int c)
{
	return startsIdentifier(c) || (c <= '9' && c >= '0') || c == '#' || c == '@';
}

static int readIdentifier(char firstChar)
{
	dbgPrint("Reading identifier or keyword\n");
	/* Lex while checking for a keyword */
	yylval.tzSym[0] = firstChar;
	uint16_t nodeID = keywordDict[0].children[dictIndex(firstChar)];
	int tokenType = firstChar == '.' ? T_LOCAL_ID : T_ID;
	size_t i = 1;

	/* Continue reading while the char is in the symbol charset */
	for (int c = peek(); continuesIdentifier(c); i++, c = peek()) {
		shiftChar();

		if (i < sizeof(yylval.tzSym) - 1) {
			/* Write the char to the identifier's name */
			yylval.tzSym[i] = c;

			/* If the char was a dot, mark the identifier as local */
			if (c == '.')
				tokenType = T_LOCAL_ID;

			/* Attempt to traverse the tree to check for a keyword */
			if (nodeID) /* Do nothing if matching already failed */
				nodeID = keywordDict[nodeID].children[dictIndex(c)];
		}
	}

	if (i > sizeof(yylval.tzSym) - 1) {
		warning(WARNING_LONG_STR, "Symbol name too long, got truncated\n");
		i = sizeof(yylval.tzSym) - 1;
	}
	yylval.tzSym[i] = '\0'; /* Terminate the string */
	dbgPrint("Ident/keyword = \"%s\"\n", yylval.tzSym);

	if (keywordDict[nodeID].keyword)
		return keywordDict[nodeID].keyword->token;

	return tokenType;
}

/* Functions to read strings */

static char const *readInterpolation(unsigned int depth)
{
	if (depth >= nMaxRecursionDepth)
		fatalerror("Recursion limit (%zu) exceeded\n", nMaxRecursionDepth);

	char symName[MAXSYMLEN + 1];
	size_t i = 0;
	struct FormatSpec fmt = fmt_NewSpec();

	for (;;) {
		int c = peek();

		if (c == '{') { /* Nested interpolation */
			shiftChar();
			char const *ptr = readInterpolation(depth + 1);

			if (ptr) {
				beginExpansion(ptr, false, ptr);
				continue; /* Restart, reading from the new buffer */
			}
		} else if (c == EOF || c == '\r' || c == '\n' || c == '"') {
			error("Missing }\n");
			break;
		} else if (c == '}') {
			shiftChar();
			break;
		} else if (c == ':' && !fmt_IsFinished(&fmt)) { /* Format spec, only once */
			shiftChar();
			for (size_t j = 0; j < i; j++)
				fmt_UseCharacter(&fmt, symName[j]);
			fmt_FinishCharacters(&fmt);
			symName[i] = '\0';
			if (!fmt_IsValid(&fmt))
				error("Invalid format spec '%s'\n", symName);
			i = 0; /* Now that format has been set, restart at beginning of string */
		} else {
			shiftChar();
			if (i < sizeof(symName)) /* Allow writing an extra char to flag overflow */
				symName[i++] = c;
		}
	}

	if (i == sizeof(symName)) {
		warning(WARNING_LONG_STR, "Symbol name too long\n");
		i--;
	}
	symName[i] = '\0';

	static char buf[MAXSTRLEN + 1];

	struct Symbol const *sym = sym_FindScopedSymbol(symName);

	if (!sym) {
		error("Interpolated symbol \"%s\" does not exist\n", symName);
	} else if (sym->type == SYM_EQUS) {
		if (fmt_IsEmpty(&fmt))
			/* No format was specified */
			fmt.type = 's';
		fmt_PrintString(buf, sizeof(buf), &fmt, sym_GetStringValue(sym));
		return buf;
	} else if (sym_IsNumeric(sym)) {
		if (fmt_IsEmpty(&fmt)) {
			/* No format was specified; default to uppercase $hex */
			fmt.type = 'X';
			fmt.prefix = true;
		}
		fmt_PrintNumber(buf, sizeof(buf), &fmt, sym_GetConstantSymValue(sym));
		return buf;
	} else {
		error("Only numerical and string symbols can be interpolated\n");
	}
	return NULL;
}

#define append_yylval_tzString(c) do { \
	char v = (c); /* Evaluate c exactly once in case it has side effects. */ \
	if (i < sizeof(yylval.tzString)) \
		yylval.tzString[i++] = v; \
} while (0)

static size_t appendEscapedSubstring(char const *str, size_t i)
{
	/* Copy one extra to flag overflow */
	while (*str) {
		char c = *str++;

		/* Escape characters that need escaping */
		switch (c) {
		case '\\':
		case '"':
		case '{':
			append_yylval_tzString('\\');
			break;
		case '\n':
			append_yylval_tzString('\\');
			c = 'n';
			break;
		case '\r':
			append_yylval_tzString('\\');
			c = 'r';
			break;
		case '\t':
			append_yylval_tzString('\\');
			c = 't';
			break;
		}

		append_yylval_tzString(c);
	}

	return i;
}

static void readString(void)
{
	dbgPrint("Reading string\n");
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	size_t i = 0;
	bool multiline = false;

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
			/* Handle CRLF before nextLine() since shiftChar updates colNo */
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
					append_yylval_tzString('"');
					break;
				}
				shiftChar();
			}
			goto finish;

		case '\\': // Character escape or macro arg
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
				shiftChar();
				char const *str = readMacroArg(c);

				while (*str)
					append_yylval_tzString(*str++);
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
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			char const *ptr = readInterpolation(0);

			if (ptr)
				while (*ptr)
					append_yylval_tzString(*ptr++);
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

		// Regular characters will just get copied
		}

		append_yylval_tzString(c);
	}

finish:
	if (i == sizeof(yylval.tzString)) {
		i--;
		warning(WARNING_LONG_STR, "String constant too long\n");
	}
	yylval.tzString[i] = '\0';

	dbgPrint("Read string \"%s\"\n", yylval.tzString);
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
}

static size_t appendStringLiteral(size_t i)
{
	dbgPrint("Reading string\n");
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	bool multiline = false;

	// We reach this function after reading a single quote, but we also support triple quotes
	append_yylval_tzString('"');
	if (peek() == '"') {
		append_yylval_tzString('"');
		shiftChar();
		if (peek() == '"') {
			// """ begins a multi-line string
			append_yylval_tzString('"');
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
			/* Handle CRLF before nextLine() since shiftChar updates colNo */
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
				append_yylval_tzString('"');
				shiftChar();
				if (peek() != '"')
					break;
				append_yylval_tzString('"');
				shiftChar();
			}
			append_yylval_tzString('"');
			goto finish;

		case '\\': // Character escape or macro arg
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
				append_yylval_tzString('\\');
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
				shiftChar();
				char const *str = readMacroArg(c);

				i = appendEscapedSubstring(str, i);
				continue; // Do not copy an additional character

			case EOF: // Can't really print that one
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

			case ',': /* `\,` inside a macro arg string literal */
				warning(WARNING_OBSOLETE,
					"`\\,` is deprecated inside strings\n");
				shiftChar();
				break;

			default:
				error("Illegal character escape %s\n", printChar(c));
				shiftChar();
				break;
			}
			break;

		case '{': // Symbol interpolation
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			char const *ptr = readInterpolation(0);

			if (ptr)
				i = appendEscapedSubstring(ptr, i);
			lexerState->disableMacroArgs = true;
			continue; // Do not copy an additional character

		// Regular characters will just get copied
		}

		append_yylval_tzString(c);
	}

finish:
	if (i == sizeof(yylval.tzString)) {
		i--;
		warning(WARNING_LONG_STR, "String constant too long\n");
	}
	yylval.tzString[i] = '\0';

	dbgPrint("Read string \"%s\"\n", yylval.tzString);
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;

	return i;
}

/* Lexer core */

static int yylex_SKIP_TO_ENDC(void); // forward declaration for yylex_NORMAL

static int yylex_NORMAL(void)
{
	dbgPrint("Lexing in normal mode, line=%" PRIu32 ", col=%" PRIu32 "\n",
		 lexer_GetLineNo(), lexer_GetColNo());

	if (lexerState->nextToken) {
		int token = lexerState->nextToken;

		lexerState->nextToken = 0;
		return token;
	}

	for (;;) {
		int c = nextChar();
		char secondChar;

		switch (c) {
		/* Ignore whitespace and comments */

		case ';':
			discardComment();
			/* fallthrough */
		case ' ':
		case '\t':
			break;

		/* Handle unambiguous single-char tokens */

		case '^':
			return T_OP_XOR;
		case '+':
			return T_OP_ADD;
		case '-':
			return T_OP_SUB;
		case '~':
			return T_OP_NOT;

		case '@':
			yylval.tzSym[0] = '@';
			yylval.tzSym[1] = '\0';
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

		/* Handle ambiguous 1- or 2-char tokens */

		case '*': /* Either MUL or EXP */
			if (peek() == '*') {
				shiftChar();
				return T_OP_EXP;
			}
			return T_OP_MUL;

		case '/': /* Either division or a block comment */
			if (peek() == '*') {
				shiftChar();
				discardBlockComment();
				break;
			}
			return T_OP_DIV;

		case '|': /* Either binary or logical OR */
			if (peek() == '|') {
				shiftChar();
				return T_OP_LOGICOR;
			}
			return T_OP_OR;

		case '=': /* Either SET alias, or EQ */
			if (peek() == '=') {
				shiftChar();
				return T_OP_LOGICEQU;
			}
			return T_POP_EQUAL;

		case '<': /* Either a LT, LTE, or left shift */
			switch (peek()) {
			case '=':
				shiftChar();
				return T_OP_LOGICLE;
			case '<':
				shiftChar();
				return T_OP_SHL;
			default:
				return T_OP_LOGICLT;
			}

		case '>': /* Either a GT, GTE, or right shift */
			switch (peek()) {
			case '=':
				shiftChar();
				return T_OP_LOGICGE;
			case '>':
				shiftChar();
				return T_OP_SHR;
			default:
				return T_OP_LOGICGT;
			}

		case '!': /* Either a NEQ, or negation */
			if (peek() == '=') {
				shiftChar();
				return T_OP_LOGICNE;
			}
			return T_OP_LOGICNOT;

		/* Handle colon, which may begin an anonymous label ref */

		case ':':
			c = peek();
			if (c != '+' && c != '-')
				return T_COLON;

			readAnonLabelRef(c);
			return T_ANON;

		/* Handle numbers */

		case '$':
			yylval.nConstValue = 0;
			readHexNumber();
			/* Attempt to match `$ff00+c` */
			if (yylval.nConstValue == 0xff00) {
				/* Whitespace is ignored anyways */
				while (isWhitespace(c = peek()))
					shiftChar();
				if (c == '+') {
					shiftChar();
					while (isWhitespace(c = peek()))
						shiftChar();
					if (c == 'c' || c == 'C') {
						shiftChar();
						return T_MODE_HW_C;
					}
					/* Retroactively lex the plus after the $ff00 */
					lexerState->nextToken = T_OP_ADD;
				}
			}
			return T_NUMBER;

		case '0': /* Decimal number */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			readNumber(10, c - '0');
			if (peek() == '.') {
				shiftChar();
				readFractionalPart();
			}
			return T_NUMBER;

		case '&':
			secondChar = peek();
			if (secondChar == '&') {
				shiftChar();
				return T_OP_LOGICAND;
			} else if (secondChar >= '0' && secondChar <= '7') {
				readNumber(8, 0);
				return T_NUMBER;
			}
			return T_OP_AND;

		case '%': /* Either a modulo, or a binary constant */
			secondChar = peek();
			if (secondChar != binDigits[0] && secondChar != binDigits[1])
				return T_OP_MOD;

			yylval.nConstValue = 0;
			readBinaryNumber();
			return T_NUMBER;

		case '`': /* Gfx constant */
			readGfxConstant();
			return T_NUMBER;

		/* Handle strings */

		case '"':
			readString();
			return T_STRING;

		/* Handle newlines and EOF */

		case '\r':
			handleCRLF(c);
			/* fallthrough */
		case '\n':
			return T_NEWLINE;

		case EOF:
			return T_EOF;

		/* Handle line continuations */

		case '\\':
			// Macro args were handled by `peek`, and character escapes do not exist
			// outside of string literals, so this must be a line continuation.
			readLineContinuation();
			break;

		/* Handle identifiers... or report garbage characters */

		default:
			if (startsIdentifier(c)) {
				int tokenType = readIdentifier(c);

				/* An ELIF after a taken IF needs to not evaluate its condition */
				if (tokenType == T_POP_ELIF && lexerState->lastToken == T_NEWLINE
				 && lexer_GetIFDepth() > 0 && lexer_RanIFBlock()
				 && !lexer_ReachedELSEBlock())
					return yylex_SKIP_TO_ENDC();

				/* If a keyword, don't try to expand */
				if (tokenType != T_ID && tokenType != T_LOCAL_ID)
					return tokenType;

				/* Local symbols cannot be string expansions */
				if (tokenType == T_ID && lexerState->expandStrings) {
					/* Attempt string expansion */
					struct Symbol const *sym = sym_FindExactSymbol(yylval.tzSym);

					if (sym && sym->type == SYM_EQUS) {
						char const *s = sym_GetStringValue(sym);

						beginExpansion(s, false, sym->name);
						continue; /* Restart, reading from the new buffer */
					}
				}

				if (tokenType == T_ID && (lexerState->atLineStart || peek() == ':'))
					return T_LABEL;

				return tokenType;
			}

			/* Do not report weird characters when capturing, it'll be done later */
			if (!lexerState->capturing) {
				/* TODO: try to group reportings */
				error("Unknown character %s\n", printChar(c));
			}
		}
		lexerState->atLineStart = false;
	}
}

static int yylex_RAW(void)
{
	dbgPrint("Lexing in raw mode, line=%" PRIu32 ", col=%" PRIu32 "\n",
		 lexer_GetLineNo(), lexer_GetColNo());

	/* This is essentially a modified `appendStringLiteral` */
	size_t i = 0;
	int c;

	/* Trim left whitespace (stops at a block comment or line continuation) */
	while (isWhitespace(peek()))
		shiftChar();

	for (;;) {
		c = peek();

		switch (c) {
		case '"': /* String literals inside macro args */
			shiftChar();
			i = appendStringLiteral(i);
			break;

		case ';': /* Comments inside macro args */
			discardComment();
			c = peek();
			/* fallthrough */
		case ',': /* End of macro arg */
		case '\r':
		case '\n':
		case EOF:
			goto finish;

		case '/': /* Block comments inside macro args */
			shiftChar();
			if (peek() == '*') {
				shiftChar();
				discardBlockComment();
				continue;
			}
			append_yylval_tzString(c); /* Append the slash */
			break;

		case '\\': /* Character escape */
			shiftChar();
			c = peek();

			switch (c) {
			case ',': /* Escape `\,` only inside a macro arg */
			case '\\': /* Escapes shared with string literals */
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

			case EOF: /* Can't really print that one */
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

			/*
			 * Macro args were already handled by peek, so '\@',
			 * '\#', and '\0'-'\9' should not occur here.
			 */

			default:
				error("Illegal character escape %s\n", printChar(c));
				break;
			}
			/* fallthrough */

		default: /* Regular characters will just get copied */
			append_yylval_tzString(c);
			shiftChar();
			break;
		}
	}

finish:
	if (i == sizeof(yylval.tzString)) {
		i--;
		warning(WARNING_LONG_STR, "Macro argument too long\n");
	}
	/* Trim right whitespace */
	while (i && isWhitespace(yylval.tzString[i - 1]))
		i--;
	yylval.tzString[i] = '\0';

	dbgPrint("Read raw string \"%s\"\n", yylval.tzString);

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

#undef append_yylval_tzString

/*
 * This function uses the fact that `if`, etc. constructs are only valid when
 * there's nothing before them on their lines. This enables filtering
 * "meaningful" (= at line start) vs. "meaningless" (everything else) tokens.
 * It's especially important due to macro args not being handled in this
 * state, and lexing them in "normal" mode potentially producing such tokens.
 */
static int skipIfBlock(bool toEndc)
{
	dbgPrint("Skipping IF block (toEndc = %s)\n", toEndc ? "true" : "false");
	lexer_SetMode(LEXER_NORMAL);
	uint32_t startingDepth = lexer_GetIFDepth();
	int token;
	bool atLineStart = lexerState->atLineStart;

	/* Prevent expanding macro args and symbol interpolation in this state */
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
					/* fallthrough */
				maybeFinish:
					if (toEndc) /* Ignore ELIF and ELSE, go to ENDC */
						break;
					/* fallthrough */
				case T_POP_ENDC:
					if (lexer_GetIFDepth() == startingDepth)
						goto finish;
					if (token == T_POP_ENDC)
						lexer_DecIFDepth();
				}
			}
			atLineStart = false;
		}

		/* Read chars until EOL */
		do {
			int c = nextChar();

			if (c == EOF) {
				token = T_EOF;
				goto finish;
			} else if (c == '\\') {
				/* Unconditionally skip the next char, including line conts */
				c = nextChar();
			} else if (c == '\r' || c == '\n') {
				atLineStart = true;
			}

			if (c == '\r' || c == '\n') {
				/* Handle CRLF before nextLine() since shiftChar updates colNo */
				handleCRLF(c);
				/* Do this both on line continuations and plain EOLs */
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

static int yylex_SKIP_TO_ELIF(void)
{
	return skipIfBlock(false);
}

static int yylex_SKIP_TO_ENDC(void)
{
	return skipIfBlock(true);
}

static int yylex_SKIP_TO_ENDR(void)
{
	dbgPrint("Skipping remainder of REPT/FOR block\n");
	lexer_SetMode(LEXER_NORMAL);
	int depth = 1;
	bool atLineStart = lexerState->atLineStart;

	/* Prevent expanding macro args and symbol interpolation in this state */
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

		/* Read chars until EOL */
		do {
			int c = nextChar();

			if (c == EOF) {
				goto finish;
			} else if (c == '\\') {
				/* Unconditionally skip the next char, including line conts */
				c = nextChar();
			} else if (c == '\r' || c == '\n') {
				atLineStart = true;
			}

			if (c == '\r' || c == '\n') {
				/* Handle CRLF before nextLine() since shiftChar updates colNo */
				handleCRLF(c);
				/* Do this both on line continuations and plain EOLs */
				nextLine();
			}
		} while (!atLineStart);
	}
finish:

	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	lexerState->atLineStart = false;

	/* yywrap() will finish the REPT/FOR loop */
	return T_EOF;
}

int yylex(void)
{
restart:
	if (lexerState->atLineStart && lexerStateEOL) {
		lexer_SetState(lexerStateEOL);
		lexerStateEOL = NULL;
	}
	if (lexerState->atLineStart) {
		/* Newlines read within an expansion should not increase the line count */
		if (!lexerState->expansions)
			nextLine();
	}

	static int (* const lexerModeFuncs[])(void) = {
		[LEXER_NORMAL]       = yylex_NORMAL,
		[LEXER_RAW]          = yylex_RAW,
		[LEXER_SKIP_TO_ELIF] = yylex_SKIP_TO_ELIF,
		[LEXER_SKIP_TO_ENDC] = yylex_SKIP_TO_ENDC,
		[LEXER_SKIP_TO_ENDR] = yylex_SKIP_TO_ENDR,
	};
	int token = lexerModeFuncs[lexerState->mode]();

	if (token == T_EOF) {
		if (lexerState->lastToken != T_NEWLINE) {
			dbgPrint("Forcing EOL at EOF\n");
			token = T_NEWLINE;
		} else {
			/* Try to switch to new buffer; if it succeeds, scan again */
			dbgPrint("Reached EOF!\n");
			/* Captures end at their buffer's boundary no matter what */
			if (!lexerState->capturing) {
				if (!yywrap())
					goto restart;
				dbgPrint("Reached end of input.\n");
				return T_EOF;
			}
		}
	}
	lexerState->lastToken = token;
	lexerState->atLineStart = token == T_NEWLINE;

	return token;
}

static char *startCapture(void)
{
	lexerState->capturing = true;
	lexerState->captureSize = 0;
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;

	if (lexerState->isMmapped && !lexerState->expansions) {
		return &lexerState->ptr[lexerState->offset];
	} else {
		lexerState->captureCapacity = 128; /* The initial size will be twice that */
		reallocCaptureBuf();
		return lexerState->captureBuf;
	}
}

void lexer_CaptureRept(struct CaptureBody *capture)
{
	capture->lineNo = lexer_GetLineNo();

	char *captureStart = startCapture();
	unsigned int level = 0;
	int c;

	/*
	 * Due to parser internals, it reads the EOL after the expression before calling this.
	 * Thus, we don't need to keep one in the buffer afterwards.
	 * The following assertion checks that.
	 */
	assert(lexerState->atLineStart);
	for (;;) {
		nextLine();
		/* We're at line start, so attempt to match a `REPT` or `ENDR` token */
		do { /* Discard initial whitespace */
			c = nextChar();
		} while (isWhitespace(c));
		/* Now, try to match `REPT`, `FOR` or `ENDR` as a **whole** identifier */
		if (startsIdentifier(c)) {
			switch (readIdentifier(c)) {
			case T_POP_REPT:
			case T_POP_FOR:
				level++;
				/* Ignore the rest of that line */
				break;

			case T_POP_ENDR:
				if (!level) {
					/*
					 * The final ENDR has been captured, but we don't want it!
					 * We know we have read exactly "ENDR", not e.g. an EQUS
					 */
					lexerState->captureSize -= strlen("ENDR");
					lexerState->lastToken = T_POP_ENDR; // Force EOL at EOF
					goto finish;
				}
				level--;
			}
		}

		/* Just consume characters until EOL or EOF */
		for (;;) {
			if (c == EOF) {
				error("Unterminated REPT/FOR block\n");
				goto finish;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
			c = nextChar();
		}
	}

finish:
	capture->body = captureStart;
	capture->size = lexerState->captureSize;
	lexerState->capturing = false;
	lexerState->captureBuf = NULL;
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	lexerState->atLineStart = false;
}

void lexer_CaptureMacroBody(struct CaptureBody *capture)
{
	capture->lineNo = lexer_GetLineNo();

	char *captureStart = startCapture();
	int c;

	/* If the file is `mmap`ed, we need not to unmap it to keep access to the macro */
	if (lexerState->isMmapped)
		lexerState->isReferenced = true;

	/*
	 * Due to parser internals, it reads the EOL after the expression before calling this.
	 * Thus, we don't need to keep one in the buffer afterwards.
	 * The following assertion checks that.
	 */
	assert(lexerState->atLineStart);
	for (;;) {
		nextLine();
		/* We're at line start, so attempt to match an `ENDM` token */
		do { /* Discard initial whitespace */
			c = nextChar();
		} while (isWhitespace(c));
		/* Now, try to match `ENDM` as a **whole** identifier */
		if (startsIdentifier(c)) {
			switch (readIdentifier(c)) {
			case T_POP_ENDM:
				/*
				 * The ENDM has been captured, but we don't want it!
				 * We know we have read exactly "ENDM", not e.g. an EQUS
				 */
				lexerState->captureSize -= strlen("ENDM");
				lexerState->lastToken = T_POP_ENDM; // Force EOL at EOF
				goto finish;
			}
		}

		/* Just consume characters until EOL or EOF */
		for (;;) {
			if (c == EOF) {
				error("Unterminated macro definition\n");
				goto finish;
			} else if (c == '\n' || c == '\r') {
				handleCRLF(c);
				break;
			}
			c = nextChar();
		}
	}

finish:
	capture->body = captureStart;
	capture->size = lexerState->captureSize;
	lexerState->capturing = false;
	lexerState->captureBuf = NULL;
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
	lexerState->atLineStart = false;
}
