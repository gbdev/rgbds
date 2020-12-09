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

#include "asm/asm.h"
#include "asm/lexer.h"
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
# include <windows.h>
# include <fileapi.h>
# include <winbase.h>
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
	{"LDIO", T_Z80_LDIO},
	{"LDH", T_Z80_LDIO},
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
	/* Handled in list of registers */
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
	{"STRSUB", T_OP_STRSUB},
	{"STRLEN", T_OP_STRLEN},
	{"STRCAT", T_OP_STRCAT},
	{"STRUPR", T_OP_STRUPR},
	{"STRLWR", T_OP_STRLWR},

	{"INCLUDE", T_POP_INCLUDE},
	{"PRINTT", T_POP_PRINTT},
	{"PRINTI", T_POP_PRINTI},
	{"PRINTV", T_POP_PRINTV},
	{"PRINTF", T_POP_PRINTF},
	{"EXPORT", T_POP_EXPORT},
	{"XDEF", T_POP_XDEF},
	{"GLOBAL", T_POP_GLOBAL},
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
	{"ENDR", T_POP_ENDR},

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
	{"EQU", T_POP_EQU},
	{"EQUS", T_POP_EQUS},

	/*  Handled before in list of CPU instructions */
	/* {"SET", T_POP_SET}, */

	{"PUSHS", T_POP_PUSHS},
	{"POPS", T_POP_POPS},
	{"PUSHO", T_POP_PUSHO},
	{"POPO", T_POP_POPO},

	{"OPT", T_POP_OPT}
};

static bool isWhitespace(int c)
{
	return c == ' ' || c == '\t';
}

#define LEXER_BUF_SIZE 42 /* TODO: determine a sane value for this */
/* This caps the size of buffer reads, and according to POSIX, passing more than SSIZE_MAX is UB */
static_assert(LEXER_BUF_SIZE <= SSIZE_MAX, "Lexer buffer size is too large");

struct Expansion {
	struct Expansion *firstChild;
	struct Expansion *next;
	char *name;
	char const *contents;
	size_t len;
	size_t totalLen;
	size_t distance; /* Distance between the beginning of this expansion and of its parent */
	uint8_t skip; /* How many extra characters to skip after the expansion is over */
};

struct LexerState {
	char const *path;

	/* mmap()-dependent IO state */
	bool isMmapped;
	union {
		struct { /* If mmap()ed */
			char *ptr; /* Technically `const` during the lexer's execution */
			off_t size;
			off_t offset;
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

	bool capturing; /* Whether the text being lexed should be captured */
	size_t captureSize; /* Amount of text captured */
	char *captureBuf; /* Buffer to send the captured text to if non-NULL */
	size_t captureCapacity; /* Size of the buffer above */

	bool disableMacroArgs;
	size_t macroArgScanDistance; /* Max distance already scanned for macro args */
	bool expandStrings;
	struct Expansion *expansions;
	size_t expansionOfs; /* Offset into the current top-level expansion (negative = before) */
};

struct LexerState *lexerState = NULL;
struct LexerState *lexerStateEOL = NULL;

static void initState(struct LexerState *state)
{
	state->mode = LEXER_NORMAL;
	state->atLineStart = true; /* yylex() will init colNo due to this */
	state->lastToken = 0;

	state->capturing = false;
	state->captureBuf = NULL;

	state->disableMacroArgs = false;
	state->macroArgScanDistance = 0;
	state->expandStrings = true;
	state->expansions = NULL;
	state->expansionOfs = 0;
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
			state->ptr = mappingAddr;
			state->size = fileInfo.st_size;
			state->offset = 0;

			if (verbose)
				printf("File %s successfully mmap()ped\n", path);
		}
	}
	if (!state->isMmapped) {
		/* Sometimes mmap() fails or isn't available, so have a fallback */
		if (verbose)
			printf("File %s opened as regular, errno reports \"%s\"\n",
			       path, strerror(errno));
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
	dbgPrint("Restarting REPT\n");
	lexerState->offset = 0;
	initState(lexerState);
	lexerState->lineNo = lineNo;
}

void lexer_DeleteState(struct LexerState *state)
{
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
} keywordDict[338] = {0}; /* Make sure to keep this correct when adding keywords! */

/* Convert a char into its index into the dict */
static inline uint8_t dictIndex(char c)
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

/*
 * The multiple evaluations of `retvar` causing side effects is INTENTIONAL, and
 * required for example by `lexer_dumpStringExpansions`. It is however only
 * evaluated once per level, and only then.
 *
 * This uses the concept of "X macros": you must #define LOOKUP_PRE_NEST and
 *  LOOKUP_POST_NEST before invoking this (and #undef them right after), and
 * those macros will be expanded at the corresponding points in the loop.
 * This is necessary because there are at least 3 places which need to iterate
 * through iterations while performing custom actions
 */
#define lookupExpansion(retvar, dist) do { \
	struct Expansion *exp = lexerState->expansions; \
	\
	for (;;) { \
		/* Find the closest expansion whose end is after the target */ \
		while (exp && exp->totalLen + exp->distance <= (dist)) { \
			(dist) -= exp->totalLen + exp->skip; \
			exp = exp->next; \
		} \
		\
		/* If there is none, or it begins after the target, return the previous level */ \
		if (!exp || exp->distance > (dist)) \
			break; \
		\
		/* We know we are inside of that expansion */ \
		(dist) -= exp->distance; /* Distances are relative to their parent */ \
		\
		/* Otherwise, register this expansion and repeat the process */ \
		LOOKUP_PRE_NEST(exp); \
		(retvar) = exp; \
		if (!exp->firstChild) /* If there are no children, this is it */ \
			break; \
		exp = exp->firstChild; \
		\
		LOOKUP_POST_NEST(exp); \
	} \
} while (0)

static struct Expansion *getExpansionAtDistance(size_t *distance)
{
	struct Expansion *expansion = NULL; /* Top level has no "previous" level */

#define LOOKUP_PRE_NEST(exp)
#define LOOKUP_POST_NEST(exp)
	struct Expansion *exp = lexerState->expansions;

	for (;;) {
		/* Find the closest expansion whose end is after the target */
		while (exp && exp->totalLen + exp->distance <= *distance) {
			*distance -= exp->totalLen - exp->skip;
			exp = exp->next;
		}

		/* If there is none, or it begins after the target, return the previous level */
		if (!exp || exp->distance > *distance)
			break;

		/* We know we are inside of that expansion */
		*distance -= exp->distance; /* Distances are relative to their parent */

		/* Otherwise, register this expansion and repeat the process */
		LOOKUP_PRE_NEST(exp);
		expansion = exp;
		if (!exp->firstChild) /* If there are no children, this is it */
			break;
		exp = exp->firstChild;

		LOOKUP_POST_NEST(exp);
	}
#undef LOOKUP_PRE_NEST
#undef LOOKUP_POST_NEST

	return expansion;
}

static void beginExpansion(size_t distance, uint8_t skip,
			   char const *str, size_t size, char const *name)
{
	distance += lexerState->expansionOfs; /* Distance argument is relative to read offset! */
	/* Increase the total length of all parents, and return the topmost one */
	struct Expansion *parent = NULL;
	unsigned int depth = 0;

#define LOOKUP_PRE_NEST(exp) (exp)->totalLen += size - skip
#define LOOKUP_POST_NEST(exp) do { \
	if (name && ++depth >= nMaxRecursionDepth) \
		fatalerror("Recursion limit (%u) exceeded\n", nMaxRecursionDepth); \
} while (0)
	lookupExpansion(parent, distance);
#undef LOOKUP_PRE_NEST
#undef LOOKUP_POST_NEST
	struct Expansion **insertPoint = parent ? &parent->firstChild : &lexerState->expansions;

	/* We know we are in none of the children expansions: add ourselves, keeping it sorted */
	while (*insertPoint && (*insertPoint)->distance < distance)
		insertPoint = &(*insertPoint)->next;

	*insertPoint = malloc(sizeof(**insertPoint));
	if (!*insertPoint)
		fatalerror("Unable to allocate new expansion: %s\n", strerror(errno));
	(*insertPoint)->firstChild = NULL;
	(*insertPoint)->next = NULL; /* Expansions are always performed left to right */
	(*insertPoint)->name = name ? strdup(name) : NULL;
	(*insertPoint)->contents = str;
	(*insertPoint)->len = size;
	(*insertPoint)->totalLen = size;
	(*insertPoint)->distance = distance;
	(*insertPoint)->skip = skip;

	/* If expansion is the new closest one, update offset */
	if (insertPoint == &lexerState->expansions)
		lexerState->expansionOfs = 0;
}

static void freeExpansion(struct Expansion *expansion)
{
	struct Expansion *child = expansion->firstChild;

	while (child) {
		struct Expansion *next = child->next;

		freeExpansion(child);
		child = next;
	}
	free(expansion->name);
	free(expansion);
}

static char const *expandMacroArg(char name, size_t distance)
{
	char const *str;

	if (name == '@')
		str = macro_GetUniqueIDStr();
	else if (name == '0')
		fatalerror("Invalid macro argument '\\0'\n");
	else
		str = macro_GetArg(name - '0');
	if (!str)
		fatalerror("Macro argument '\\%c' not defined\n", name);

	beginExpansion(distance, 2, str, strlen(str), NULL);
	return str;
}

/* If at any point we need more than 255 characters of lookahead, something went VERY wrong. */
static int peekInternal(uint8_t distance)
{
	if (distance >= LEXER_BUF_SIZE)
		fatalerror("Internal lexer error: buffer has insufficient size for peeking (%"
			   PRIu8 " >= %u)\n", distance, LEXER_BUF_SIZE);

	size_t ofs = lexerState->expansionOfs + distance;
	struct Expansion const *expansion = getExpansionAtDistance(&ofs);

	if (expansion) {
		assert(ofs < expansion->len);
		return expansion->contents[ofs];
	}

	distance = ofs;

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
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path, errno); \
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
			/* If the read was incomplete, don't perform a second read */
			if (nbCharsRead < nbExpectedChars)
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

static int peek(uint8_t distance)
{
	int c = peekInternal(distance);

	if (distance >= lexerState->macroArgScanDistance) {
		lexerState->macroArgScanDistance = distance + 1; /* Do not consider again */
		/* If enabled and character is a backslash, check for a macro arg */
		if (!lexerState->disableMacroArgs && c == '\\') {
			distance++;
			lexerState->macroArgScanDistance++;
			c = peekInternal(distance);
			if (c == '@' || (c >= '0' && c <= '9')) {
				/* Expand the argument and return its first character */
				char const *str = expandMacroArg(c, distance - 1);

				/*
				 * Assuming macro args can't be recursive (I'll be damned if a way
				 * is found...), then we mark the entire macro arg as scanned;
				 * however, the two macro arg characters (\1) will be ignored,
				 * so they shouldn't be counted in the scan distance!
				 */
				lexerState->macroArgScanDistance += strlen(str) - 2;
				/* WARNING: this assumes macro args can't be empty!! */
				c = str[0];
			} else {
				c = '\\';
			}
		}
	}
	return c;
}

static void shiftChars(uint8_t distance)
{
	if (lexerState->capturing) {
		if (lexerState->captureBuf) {
			if (lexerState->captureSize + distance >= lexerState->captureCapacity)
				reallocCaptureBuf();
			/* TODO: improve this? */
			for (uint8_t i = 0; i < distance; i++)
				lexerState->captureBuf[lexerState->captureSize++] = peek(i);
		} else {
			lexerState->captureSize += distance;
		}
	}

	lexerState->macroArgScanDistance -= distance;

	/* FIXME: this may not be too great, as only the top level is considered... */

	/*
	 * The logic is as follows:
	 * - Any characters up to the expansion need to be consumed in the file
	 * - If some remain after that, advance the offset within the expansion
	 * - If that goes *past* the expansion, then leftovers shall be consumed in the file
	 * - If we went past the expansion, we're back to square one, and should re-do all
	 */
nextExpansion:
	if (lexerState->expansions) {
		/* If the read cursor reaches into the expansion, update offset */
		if (distance > lexerState->expansions->distance) {
			/* distance = <file chars (expansion distance)> + <expansion chars> */
			lexerState->expansionOfs += distance - lexerState->expansions->distance;
			distance = lexerState->expansions->distance; /* Nb chars to read in file */
			/* Now, check if the expansion finished being read */
			if (lexerState->expansionOfs >= lexerState->expansions->totalLen) {
				/* Add the leftovers to the distance */
				distance += lexerState->expansionOfs;
				distance -= lexerState->expansions->totalLen;
				/* Also add in the post-expansion skip */
				distance += lexerState->expansions->skip;
				/* Move on to the next expansion */
				struct Expansion *next = lexerState->expansions->next;

				freeExpansion(lexerState->expansions);
				lexerState->expansions = next;
				/* Reset the offset for the next expansion */
				lexerState->expansionOfs = 0;
				/* And repeat, in case we also go into or over the next expansion */
				goto nextExpansion;
			}
		}
		/* Getting closer to the expansion */
		lexerState->expansions->distance -= distance;
		/* Now, `distance` is how many bytes to move forward **in the file** */
	}

	if (lexerState->isMmapped) {
		lexerState->offset += distance;
	} else {
		lexerState->index += distance;
		lexerState->colNo += distance;
		/* Wrap around if necessary */
		if (lexerState->index >= LEXER_BUF_SIZE)
			lexerState->index %= LEXER_BUF_SIZE;
		assert(lexerState->nbChars >= distance);
		lexerState->nbChars -= distance;
	}
}

static int nextChar(void)
{
	int c = peek(0);

	/* If not at EOF, advance read position */
	if (c != EOF)
		shiftChars(1);
	return c;
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
	struct Expansion **stack = malloc(sizeof(*stack) * (nMaxRecursionDepth + 1));
	struct Expansion *expansion; /* Temp var for `lookupExpansion` */
	unsigned int depth = 0;
	size_t distance = lexerState->expansionOfs;

	if (!stack)
		fatalerror("Failed to alloc string expansion stack: %s\n", strerror(errno));

#define LOOKUP_PRE_NEST(exp) do { \
	/* Only register EQUS expansions, not string args */ \
	if ((exp)->name) \
		stack[depth++] = (exp); \
} while (0)
#define LOOKUP_POST_NEST(exp)
	lookupExpansion(expansion, distance);
	(void)expansion;
#undef LOOKUP_PRE_NEST
#undef LOOKUP_POST_NEST

	while (depth--)
		fprintf(stderr, "while expanding symbol \"%s\"\n", stack[depth]->name);
	free(stack);
}

/* Discards an block comment */
static void discardBlockComment(void)
{
	dbgPrint("Discarding block comment\n");
	for (;;) {
		switch (nextChar()) {
		case EOF:
			error("Unterminated block comment\n");
			return;
		case '/':
			if (peek(0) == '*') {
				warning(WARNING_NESTED_COMMENT,
					"/* in block comment\n");
			}
			continue;
		case '*':
			if (peek(0) == '/') {
				shiftChars(1);
				return;
			}
			/* fallthrough */
		default:
			continue;
		}
	}
}

/* Function to discard all of a line's comments */

static void discardComment(void)
{
	dbgPrint("Discarding comment\n");
	lexerState->disableMacroArgs = true;
	for (;;) {
		int c = peek(0);

		if (c == EOF || c == '\r' || c == '\n')
			break;
		shiftChars(1);
	}
	lexerState->disableMacroArgs = false;
}

/* Function to read a line continuation */

static void readLineContinuation(void)
{
	dbgPrint("Beginning line continuation\n");
	for (;;) {
		int c = peek(0);

		if (isWhitespace(c)) {
			shiftChars(1);
		} else if (c == '\r' || c == '\n') {
			shiftChars(1);
			if (c == '\r' && peek(0) == '\n')
				shiftChars(1);
			if (!lexerState->expansions
			 || lexerState->expansions->distance)
				lexerState->lineNo++;
			return;
		} else if (c == ';') {
			discardComment();
		} else {
			error("Begun line continuation, but encountered character '%s'\n",
			      print(c));
			return;
		}
	}
}

/* Functions to lex numbers of various radixes */

static void readNumber(int radix, int32_t baseValue)
{
	uint32_t value = baseValue;

	for (;;) {
		int c = peek(0);

		if (c < '0' || c > '0' + radix - 1)
			break;
		if (value > (UINT32_MAX - (c - '0')) / radix)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * radix + (c - '0');

		shiftChars(1);
	}

	yylval.nConstValue = value;
}

static void readFractionalPart(void)
{
	uint32_t value = 0, divisor = 1;

	dbgPrint("Reading fractional part\n");
	for (;;) {
		int c = peek(0);

		if (c < '0' || c > '9')
			break;
		shiftChars(1);
		if (divisor > (UINT32_MAX - (c - '0')) / 10) {
			warning(WARNING_LARGE_CONSTANT,
				"Precision of fixed-point constant is too large\n");
			/* Discard any additional digits */
			while (c = peek(0), c >= '0' && c <= '9')
				shiftChars(1);
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
	uint16_t fractional = value * 65536 / divisor;

	yylval.nConstValue |= fractional * (yylval.nConstValue >= 0 ? 1 : -1);
}

char const *binDigits;

static void readBinaryNumber(void)
{
	uint32_t value = 0;

	dbgPrint("Reading binary number with digits [%c,%c]\n", binDigits[0], binDigits[1]);
	for (;;) {
		int c = peek(0);
		int bit;

		if (c == binDigits[0])
			bit = 0;
		else if (c == binDigits[1])
			bit = 1;
		else
			break;
		if (value > (UINT32_MAX - bit) / 2)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * 2 + bit;

		shiftChars(1);
	}

	yylval.nConstValue = value;
}

static void readHexNumber(void)
{
	uint32_t value = 0;
	bool empty = true;

	dbgPrint("Reading hex number\n");
	for (;;) {
		int c = peek(0);

		if (c >= 'a' && c <= 'f') /* Convert letters to right after digits */
			c = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			c = c - 'A' + 10;
		else if (c >= '0' && c <= '9')
			c = c - '0';
		else
			break;

		if (value > (UINT32_MAX - c) / 16)
			warning(WARNING_LARGE_CONSTANT, "Integer constant is too large\n");
		value = value * 16 + c;

		shiftChars(1);
		empty = false;
	}

	if (empty)
		error("Invalid integer constant, no digits after '$'\n");

	yylval.nConstValue = value;
}

char const *gfxDigits;

static void readGfxConstant(void)
{
	uint32_t bp0 = 0, bp1 = 0;
	uint8_t width = 0;

	dbgPrint("Reading gfx constant with digits [%c,%c,%c,%c]\n",
		 gfxDigits[0], gfxDigits[1], gfxDigits[2], gfxDigits[3]);
	for (;;) {
		int c = peek(0);
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
		shiftChars(1);
	}

	if (width == 0)
		error("Invalid graphics constant, no digits after '`'\n");
	else if (width == 9)
		warning(WARNING_LARGE_CONSTANT,
			"Graphics constant is too long, only 8 first pixels considered\n");

	yylval.nConstValue = bp1 << 8 | bp0;
}

/* Function to read identifiers & keywords */

static bool startsIdentifier(int c)
{
	return (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || c == '.' || c == '_';
}

static int readIdentifier(char firstChar)
{
	dbgPrint("Reading identifier or keyword\n");
	/* Lex while checking for a keyword */
	yylval.tzSym[0] = firstChar;
	uint16_t nodeID = keywordDict[0].children[dictIndex(firstChar)];
	int tokenType = firstChar == '.' ? T_LOCAL_ID : T_ID;
	size_t i;

	for (i = 1; ; i++) {
		int c = peek(0);

		/* If that char isn't in the symbol charset, end */
		if ((c > '9' || c < '0')
		 && (c > 'Z' || c < 'A')
		 && (c > 'z' || c < 'a')
		 && c != '#' && c != '.' && c != '@' && c != '_')
			break;
		shiftChars(1);

		/* Write the char to the identifier's name */
		if (i < sizeof(yylval.tzSym) - 1)
			yylval.tzSym[i] = c;

		/* If the char was a dot, mark the identifier as local */
		if (c == '.')
			tokenType = T_LOCAL_ID;

		/* Attempt to traverse the tree to check for a keyword */
		if (nodeID) /* Do nothing if matching already failed */
			nodeID = keywordDict[nodeID].children[dictIndex(c)];
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

enum PrintType {
	TYPE_NONE,
	TYPE_DECIMAL,  /* d */
	TYPE_UPPERHEX, /* X */
	TYPE_LOWERHEX, /* x */
	TYPE_BINARY,   /* b */
};

static void intToString(char *dest, size_t bufSize, struct Symbol const *sym, enum PrintType type)
{
	uint32_t value = sym_GetConstantSymValue(sym);
	int fullLength;

	/* Special cheat for binary */
	if (type == TYPE_BINARY) {
		char binary[33]; /* 32 bits + 1 terminator */
		char *write_ptr = binary + 32;

		fullLength = 0;
		binary[32] = 0;
		do {
			*(--write_ptr) = (value & 1) + '0';
			value >>= 1;
			fullLength++;
		} while (value);
		strncpy(dest, write_ptr, bufSize - 1);
	} else {
		static char const * const formats[] = {
			[TYPE_NONE]     = "$%" PRIX32,
			[TYPE_DECIMAL]  = "%" PRId32,
			[TYPE_UPPERHEX] = "%" PRIX32,
			[TYPE_LOWERHEX] = "%" PRIx32
		};

		fullLength = snprintf(dest, bufSize, formats[type], value);
		if (fullLength < 0) {
			error("snprintf encoding error: %s\n", strerror(errno));
			dest[0] = '\0';
		}
	}

	if ((size_t)fullLength >= bufSize)
		warning(WARNING_LONG_STR, "Interpolated symbol %s too long to fit buffer\n",
			sym->name);
}

static char const *readInterpolation(void)
{
	char symName[MAXSYMLEN + 1];
	size_t i = 0;
	enum PrintType type = TYPE_NONE;

	for (;;) {
		int c = peek(0);

		if (c == '{') { /* Nested interpolation */
			shiftChars(1);
			char const *inner = readInterpolation();

			if (inner) {
				while (*inner) {
					if (i == sizeof(symName))
						break;
					symName[i++] = *inner++;
				}
			}
		} else if (c == EOF || c == '\r' || c == '\n' || c == '"') {
			error("Missing }\n");
			break;
		} else if (c == '}') {
			shiftChars(1);
			break;
		} else if (c == ':' && type == TYPE_NONE) { /* Print type, only once */
			if (i != 1) {
				error("Print types are exactly 1 character long\n");
			} else {
				switch (symName[0]) {
				case 'b':
					type = TYPE_BINARY;
					break;
				case 'd':
					type = TYPE_DECIMAL;
					break;
				case 'X':
					type = TYPE_UPPERHEX;
					break;
				case 'x':
					type = TYPE_LOWERHEX;
					break;
				default:
					error("Invalid print type '%s'\n", print(symName[0]));
				}
			}
			i = 0; /* Now that type has been set, restart at beginning of string */
			shiftChars(1);
		} else {
			if (i < sizeof(symName)) /* Allow writing an extra char to flag overflow */
				symName[i++] = c;
			shiftChars(1);
		}
	}

	if (i == sizeof(symName)) {
		warning(WARNING_LONG_STR, "Symbol name too long\n");
		i--;
	}
	symName[i] = '\0';

	struct Symbol const *sym = sym_FindScopedSymbol(symName);

	if (!sym) {
		error("Interpolated symbol \"%s\" does not exist\n", symName);
	} else if (sym->type == SYM_EQUS) {
		if (type != TYPE_NONE)
			error("Print types are only allowed for numbers\n");
		return sym_GetStringValue(sym);
	} else if (sym_IsNumeric(sym)) {
		static char buf[33]; /* Worst case of 32 digits + terminator */

		intToString(buf, sizeof(buf), sym, type);
		return buf;
	} else {
		error("Only numerical and string symbols can be interpolated\n");
	}
	return NULL;
}

static void readString(void)
{
	size_t i = 0;

	dbgPrint("Reading string\n");
	for (;;) {
		int c = peek(0);

		switch (c) {
		case '"':
			shiftChars(1);
			if (i == sizeof(yylval.tzString)) {
				i--;
				warning(WARNING_LONG_STR, "String constant too long\n");
			}
			yylval.tzString[i] = '\0';
			dbgPrint("Read string \"%s\"\n", yylval.tzString);
			return;
		case '\r':
		case '\n': /* Do not shift these! */
		case EOF:
			if (i == sizeof(yylval.tzString)) {
				i--;
				warning(WARNING_LONG_STR, "String constant too long\n");
			}
			yylval.tzString[i] = '\0';
			error("Unterminated string\n");
			dbgPrint("Read string \"%s\"\n", yylval.tzString);
			return;

		case '\\': /* Character escape */
			c = peek(1);
			switch (c) {
			case '\\': /* Return that character unchanged */
			case '"':
			case '{':
			case '}':
				shiftChars(1);
				break;
			case 'n':
				c = '\n';
				shiftChars(1);
				break;
			case 'r':
				c = '\r';
				shiftChars(1);
				break;
			case 't':
				c = '\t';
				shiftChars(1);
				break;

			case ' ':
			case '\r':
			case '\n':
				shiftChars(1); /* Shift the backslash */
				readLineContinuation();
				continue;

			case EOF: /* Can't really print that one */
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;
			default:
				error("Illegal character escape '%s'\n", print(c));
				c = '\\';
				break;
			}
			break;

		case '{': /* Symbol interpolation */
			shiftChars(1);
			char const *ptr = readInterpolation();

			if (ptr) {
				while (*ptr) {
					if (i == sizeof(yylval.tzString))
						break;
					yylval.tzString[i++] = *ptr++;
				}
			}
			continue; /* Do not copy an additional character */

		/* Regular characters will just get copied */
		}
		if (i < sizeof(yylval.tzString)) /* Copy one extra to flag overflow */
			yylval.tzString[i++] = c;
		shiftChars(1);
	}
}

/* Function to report one character's worth of garbage bytes */

static char const *reportGarbageChar(unsigned char firstByte)
{
	static char bytes[6 + 2 + 1]; /* Max size of a UTF-8 encoded code point, plus "''\0" */
	/* First, attempt UTF-8 decoding */
	uint32_t state = 0; /* UTF8_ACCEPT */
	uint32_t codepoint;
	uint8_t size = 0; /* Number of additional bytes to shift */

	bytes[1] = firstByte; /* No need to init the rest of the array */
	decode(&state, &codepoint, firstByte);
	while (state != 0 && state != 1 /* UTF8_REJECT */) {
		int c = peek(size++);

		if (c == EOF)
			break;
		bytes[size + 1] = c;
		decode(&state, &codepoint, c);
	}

	if (state == 0 && (codepoint > UCHAR_MAX || isprint((unsigned char)codepoint))) {
		/* Character is valid, printable UTF-8! */
		shiftChars(size);
		bytes[0] = '\'';
		bytes[size + 2] = '\'';
		bytes[size + 3] = '\0';
		return bytes;
	}

	/* The character isn't valid UTF-8, so we'll only print that first byte */
	if (isprint(firstByte)) {
		/* bytes[1] = firstByte; */
		bytes[0] = '\'';
		bytes[2] = '\'';
		bytes[3] = '\0';
		return bytes;
	}
	/* Well then, print its hex value */
	static char const hexChars[16] = "0123456789ABCDEF";

	bytes[0] = '0';
	bytes[1] = 'x';
	bytes[2] = hexChars[firstByte >> 4];
	bytes[3] = hexChars[firstByte & 0x0f];
	bytes[4] = '\0';
	return bytes;
}

/* Lexer core */

static int yylex_NORMAL(void)
{
	dbgPrint("Lexing in normal mode, line=%" PRIu32 ", col=%" PRIu32 "\n",
		 lexer_GetLineNo(), lexer_GetColNo());
	for (;;) {
		int c = nextChar();

		switch (c) {
		/* Ignore whitespace and comments */

		case '*':
			if (!lexerState->atLineStart)
				return T_OP_MUL;
			warning(WARNING_OBSOLETE,
				"'*' is deprecated for comments, please use ';' instead\n");
			/* fallthrough */
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

		/* Handle accepted single chars */

		case '[':
		case ']':
		case '(':
		case ')':
		case ',':
		case ':':
			return c;

		/* Handle ambiguous 1- or 2-char tokens */
		char secondChar;
		case '/': /* Either division or a block comment */
			secondChar = peek(0);
			if (secondChar == '*') {
				shiftChars(1);
				discardBlockComment();
				break;
			}
			return T_OP_DIV;
		case '|': /* Either binary or logical OR */
			secondChar = peek(0);
			if (secondChar == '|') {
				shiftChars(1);
				return T_OP_LOGICOR;
			}
			return T_OP_OR;

		case '=': /* Either SET alias, or EQ */
			secondChar = peek(0);
			if (secondChar == '=') {
				shiftChars(1);
				return T_OP_LOGICEQU;
			}
			return T_POP_EQUAL;

		case '<': /* Either a LT, LTE, or left shift */
			secondChar = peek(0);
			if (secondChar == '=') {
				shiftChars(1);
				return T_OP_LOGICLE;
			} else if (secondChar == '<') {
				shiftChars(1);
				return T_OP_SHL;
			}
			return T_OP_LOGICLT;

		case '>': /* Either a GT, GTE, or right shift */
			secondChar = peek(0);
			if (secondChar == '=') {
				shiftChars(1);
				return T_OP_LOGICGE;
			} else if (secondChar == '>') {
				shiftChars(1);
				return T_OP_SHR;
			}
			return T_OP_LOGICGT;

		case '!': /* Either a NEQ, or negation */
			secondChar = peek(0);
			if (secondChar == '=') {
				shiftChars(1);
				return T_OP_LOGICNE;
			}
			return T_OP_LOGICNOT;

		/* Handle numbers */

		case '$':
			yylval.nConstValue = 0;
			readHexNumber();
			/* Attempt to match `$ff00+c` */
			if (yylval.nConstValue == 0xff00) {
				/* Whitespace is ignored anyways */
				while (isWhitespace(c = peek(0)))
					shiftChars(1);
				if (c == '+') {
					/* FIXME: not great due to large lookahead */
					uint8_t distance = 1;

					do {
						c = peek(distance++);
					} while (isWhitespace(c));

					if (c == 'c' || c == 'C') {
						shiftChars(distance);
						return T_MODE_HW_C;
					}
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
			if (peek(0) == '.') {
				shiftChars(1);
				readFractionalPart();
			}
			return T_NUMBER;

		case '&':
			secondChar = peek(0);
			if (secondChar == '&') {
				shiftChars(1);
				return T_OP_LOGICAND;
			} else if (secondChar >= '0' && secondChar <= '7') {
				readNumber(8, 0);
				return T_NUMBER;
			}
			return T_OP_AND;

		case '%': /* Either a modulo, or a binary constant */
			secondChar = peek(0);
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
			return '\r';
		case '\n':
			return '\n';

		case EOF:
			return 0;

		/* Handle escapes */

		case '\\':
			c = peek(0);

			switch (c) {
			case ' ':
			case '\r':
			case '\n':
				readLineContinuation();
				break;

			case EOF:
				error("Illegal character escape at end of input\n");
				break;
			default:
				shiftChars(1);
				error("Illegal character escape '%s'\n", print(c));
			}
			break;

		/* Handle identifiers and escapes... or error out */

		default:
			if (startsIdentifier(c)) {
				int tokenType = readIdentifier(c);

				/* If a keyword, don't try to expand */
				if (tokenType != T_ID && tokenType != T_LOCAL_ID)
					return tokenType;

				/* Local symbols cannot be string expansions */
				if (tokenType == T_ID && lexerState->expandStrings) {
					/* Attempt string expansion */
					struct Symbol const *sym = sym_FindExactSymbol(yylval.tzSym);

					if (sym && sym->type == SYM_EQUS) {
						char const *s = sym_GetStringValue(sym);

						beginExpansion(0, 0, s, strlen(s), sym->name);
						continue; /* Restart, reading from the new buffer */
					}
				}

				if (tokenType == T_ID && lexerState->atLineStart)
					return T_LABEL;

				return tokenType;
			}

			/* Do not report weird characters when capturing, it'll be done later */
			if (!lexerState->capturing) {
				/* TODO: try to group reportings */
				error("Unknown character %s\n", reportGarbageChar(c));
			}
		}
		lexerState->atLineStart = false;
	}
}

static int yylex_RAW(void)
{
	dbgPrint("Lexing in raw mode, line=%" PRIu32 ", col=%" PRIu32 "\n",
		 lexer_GetLineNo(), lexer_GetColNo());

	/* This is essentially a modified `readString` */
	size_t i = 0;
	bool insideString = false;

	/* Trim left of string... */
	while (isWhitespace(peek(0)))
		shiftChars(1);

	for (;;) {
		int c = peek(0);

		switch (c) {
		case '"':
			insideString = !insideString;
			/* Other than that, just process quotes normally */
			break;

		case ';': /* Comments inside macro args */
			if (insideString)
				break;
			discardComment();
			c = peek(0);
			/* fallthrough */
		case ',':
		case '\r':
		case '\n':
		case EOF:
			if (i == sizeof(yylval.tzString)) {
				i--;
				warning(WARNING_LONG_STR, "Macro argument too long\n");
			}
			/* Trim whitespace */
			while (i && isWhitespace(yylval.tzString[i - 1]))
				i--;
			/* Empty macro args break their expansion, so prevent that */
			if (i == 0) {
				/* Return the EOF token, and don't shift a non-existent char! */
				if (c == EOF)
					return 0;
				shiftChars(1);
				return c;
			}
			yylval.tzString[i] = '\0';
			dbgPrint("Read raw string \"%s\"\n", yylval.tzString);
			return T_STRING;

		case '\\': /* Character escape */
			c = peek(1);
			switch (c) {
			case ',':
				shiftChars(1);
				break;

			case ' ':
			case '\r':
			case '\n':
				shiftChars(1); /* Shift the backslash */
				readLineContinuation();
				continue;

			case EOF: /* Can't really print that one */
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;
			default: /* Pass the rest as-is */
				c = '\\';
				break;
			}
			break;

		case '{': /* Symbol interpolation */
			shiftChars(1);
			char const *ptr = readInterpolation();

			if (ptr) {
				while (*ptr) {
					if (i == sizeof(yylval.tzString))
						break;
					yylval.tzString[i++] = *ptr++;
				}
			}
			continue; /* Do not copy an additional character */

		/* Regular characters will just get copied */
		}
		if (i < sizeof(yylval.tzString)) /* Copy one extra to flag overflow */
			yylval.tzString[i++] = c;
		shiftChars(1);
	}
}

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
	int startingDepth = nIFDepth;
	int token;
	bool atLineStart = lexerState->atLineStart;

	/* Prevent expanding macro args in this state */
	lexerState->disableMacroArgs = true;

	for (;;) {
		if (atLineStart) {
			int c;

			for (;;) {
				c = peek(0);
				if (!isWhitespace(c))
					break;
				shiftChars(1);
			}

			if (startsIdentifier(c)) {
				shiftChars(1);
				token = readIdentifier(c);
				switch (token) {
				case T_POP_IF:
					nIFDepth++;
					break;

				case T_POP_ELIF:
				case T_POP_ELSE:
					if (toEndc) /* Ignore ELIF and ELSE, go to ENDC */
						break;
					/* fallthrough */
				case T_POP_ENDC:
					if (nIFDepth == startingDepth)
						goto finish;
					if (token == T_POP_ENDC)
						nIFDepth--;
				}
			}
			atLineStart = false;
		}

		/* Read chars until EOL */
		do {
			int c = nextChar();

			if (c == EOF) {
				token = 0;
				goto finish;
			} else if (c == '\\') {
				/* Unconditionally skip the next char, including line conts */
				c = nextChar();
			} else if (c == '\r' || c == '\n') {
				atLineStart = true;
			}

			if (c == '\r' || c == '\n')
				/* Do this both on line continuations and plain EOLs */
				lexerState->lineNo++;
			/* Handle CRLF */
			if (c == '\r' && peek(0) == '\n')
				shiftChars(1);
		} while (!atLineStart);
	}
finish:

	lexerState->disableMacroArgs = false;
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

int yylex(void)
{
restart:
	if (lexerState->atLineStart && lexerStateEOL) {
		lexer_SetState(lexerStateEOL);
		lexerStateEOL = NULL;
	}
	if (lexerState->atLineStart) {
		/* Newlines read within an expansion should not increase the line count */
		if (!lexerState->expansions || lexerState->expansions->distance) {
			lexerState->lineNo++;
			lexerState->colNo = 0;
		}
	}

	static int (* const lexerModeFuncs[])(void) = {
		[LEXER_NORMAL]       = yylex_NORMAL,
		[LEXER_RAW]          = yylex_RAW,
		[LEXER_SKIP_TO_ELIF] = yylex_SKIP_TO_ELIF,
		[LEXER_SKIP_TO_ENDC] = yylex_SKIP_TO_ENDC
	};
	int token = lexerModeFuncs[lexerState->mode]();

	/* Make sure to terminate files with a line feed */
	if (token == 0) {
		if (lexerState->lastToken != '\n') {
			dbgPrint("Forcing EOL at EOF\n");
			token = '\n';
		} else { /* Try to switch to new buffer; if it succeeds, scan again */
			dbgPrint("Reached EOF!\n");
			/* Captures end at their buffer's boundary no matter what */
			if (!lexerState->capturing) {
				if (!yywrap())
					goto restart;
				dbgPrint("Reached end of input.");
				return 0;
			}
		}
	} else if (token == '\r') { /* Handle CR and CRLF line endings */
		token = '\n'; /* We universally use '\n' as the value for line ending tokens */
		if (peek(0) == '\n')
			shiftChars(1); /* Shift the CRLF's LF */
	}
	lexerState->lastToken = token;

	lexerState->atLineStart = false;
	if (token == '\n')
		lexerState->atLineStart = true;

	return token;
}

static char *startCapture(void)
{
	lexerState->capturing = true;
	lexerState->captureSize = 0;
	lexerState->disableMacroArgs = true;

	if (lexerState->isMmapped && !lexerState->expansions) {
		return &lexerState->ptr[lexerState->offset];
	} else {
		lexerState->captureCapacity = 128; /* The initial size will be twice that */
		reallocCaptureBuf();
		return lexerState->captureBuf;
	}
}

void lexer_CaptureRept(char **capture, size_t *size)
{
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
		lexerState->lineNo++;
		/* We're at line start, so attempt to match a `REPT` or `ENDR` token */
		do { /* Discard initial whitespace */
			c = nextChar();
		} while (isWhitespace(c));
		/* Now, try to match either `REPT` or `ENDR` as a **whole** identifier */
		if (startsIdentifier(c)) {
			switch (readIdentifier(c)) {
			case T_POP_REPT:
				level++;
				/* Ignore the rest of that line */
				break;

			case T_POP_ENDR:
				if (!level) {
					/* Read (but don't capture) until EOL or EOF */
					lexerState->capturing = false;
					do {
						c = nextChar();
					} while (c != EOF && c != '\r' && c != '\n');
					/* Handle Windows CRLF */
					if (c == '\r' && peek(0) == '\n')
						shiftChars(1);
					goto finish;
				}
				level--;
			}
		}

		/* Just consume characters until EOL or EOF */
		for (;;) {
			if (c == EOF) {
				error("Unterminated REPT block\n");
				lexerState->capturing = false;
				goto finish;
			} else if (c == '\n') {
				break;
			} else if (c == '\r') {
				if (peek(0) == '\n')
					shiftChars(1);
				break;
			}
			c = nextChar();
		}
	}

finish:
	assert(!lexerState->capturing);
	*capture = captureStart;
	*size = lexerState->captureSize - strlen("ENDR");
	lexerState->captureBuf = NULL;
	lexerState->disableMacroArgs = false;
}

void lexer_CaptureMacroBody(char **capture, size_t *size)
{
	char *captureStart = startCapture();
	int c = peek(0);

	/* If the file is `mmap`ed, we need not to unmap it to keep access to the macro */
	if (lexerState->isMmapped)
		lexerState->isReferenced = true;

	/*
	 * Due to parser internals, it does not read the EOL after the T_POP_MACRO before calling
	 * this. Thus, we need to keep one in the buffer afterwards.
	 * (Note that this also means the captured buffer begins with a newline and maybe comment)
	 * The following assertion checks that.
	 */
	assert(!lexerState->atLineStart);
	for (;;) {
		/* Just consume characters until EOL or EOF */
		for (;;) {
			if (c == EOF) {
				error("Unterminated macro definition\n");
				lexerState->capturing = false;
				goto finish;
			} else if (c == '\n') {
				break;
			} else if (c == '\r') {
				if (peek(0) == '\n')
					shiftChars(1);
				break;
			}
			c = nextChar();
		}

		/* We're at line start, attempt to match a `label: MACRO` line or `ENDM` token */
		do { /* Discard initial whitespace */
			c = nextChar();
		} while (isWhitespace(c));
		/* Now, try to match either `REPT` or `ENDR` as a **whole** identifier */
		if (startsIdentifier(c)) {
			if (readIdentifier(c) == T_POP_ENDM) {
				/* Read (but don't capture) until EOL or EOF */
				lexerState->capturing = false;
				do {
					c = peek(0);
					if (c == EOF || c == '\r' || c == '\n')
						break;
					shiftChars(1);
				} while (c != EOF && c != '\r' && c != '\n');
				/* Handle Windows CRLF */
				if (c == '\r' && peek(1) == '\n')
					shiftChars(1);
				goto finish;
			}
		}
		lexerState->lineNo++;
	}

finish:
	assert(!lexerState->capturing);
	*capture = captureStart;
	*size = lexerState->captureSize - strlen("ENDM");
	lexerState->captureBuf = NULL;
	lexerState->disableMacroArgs = false;
}
