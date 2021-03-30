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
	{"PC", T_MODE_PC},
	{"HLD", T_MODE_HL_DEC},
	{"HLI", T_MODE_HL_INC},
	{"IME", T_MODE_IME},

	{"A", T_TOKEN_A},
	{"F", T_TOKEN_F},
	{"B", T_TOKEN_B},
	{"C", T_TOKEN_C},
	{"D", T_TOKEN_D},
	{"E", T_TOKEN_E},
	{"H", T_TOKEN_H},
	{"L", T_TOKEN_L},
	{"W", T_TOKEN_W},

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
	{"...", T_ELLIPSIS},
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
	union {
		char const *unowned;
		char *owned;
	} contents;
	size_t len;
	size_t totalLen;
	size_t distance; /* Distance between the beginning of this expansion and of its parent */
	uint8_t skip; /* How many extra characters to skip after the expansion is over */
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
	struct Expansion *expansions;
	size_t expansionOfs; /* Offset into the current top-level expansion (negative = before) */
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
	state->expansionOfs = 0;
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
			state->size = fileInfo.st_size;
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
} keywordDict[356] = {0}; /* Make sure to keep this correct when adding keywords! */

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
			   char const *str, size_t size, bool owned,
			   char const *name)
{
	distance += lexerState->expansionOfs; /* Distance argument is relative to read offset! */
	/* Increase the total length of all parents, and return the topmost one */
	struct Expansion *parent = NULL;
	unsigned int depth = 0;

#define LOOKUP_PRE_NEST(exp) (exp)->totalLen += size - skip
#define LOOKUP_POST_NEST(exp) do { \
	if (name && ++depth >= nMaxRecursionDepth) \
		fatalerror("Recursion limit (%zu) exceeded\n", nMaxRecursionDepth); \
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
	(*insertPoint)->contents.unowned = str;
	(*insertPoint)->len = size;
	(*insertPoint)->totalLen = size;
	(*insertPoint)->distance = distance;
	(*insertPoint)->skip = skip;
	(*insertPoint)->owned = owned;

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
		return expansion->contents.unowned[ofs];
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
static void shiftChars(uint8_t distance);
static char const *readInterpolation(void);

static int peek(uint8_t distance)
{
	int c;

restart:
	c = peekInternal(distance);

	if (distance >= lexerState->macroArgScanDistance) {
		lexerState->macroArgScanDistance = distance + 1; /* Do not consider again */
		if (c == '\\' && !lexerState->disableMacroArgs) {
			/* If character is a backslash, check for a macro arg */
			lexerState->macroArgScanDistance++;
			c = peekInternal(distance + 1);
			if (isMacroChar(c)) {
				char const *str = readMacroArg(c);

				/*
				 * If the macro arg is an empty string, it cannot be
				 * expanded, so skip it and keep peeking.
				 */
				if (!str[0]) {
					shiftChars(2);
					goto restart;
				}

				beginExpansion(distance, 2, str, strlen(str), c == '#', NULL);

				/*
				 * Assuming macro args can't be recursive (I'll be damned if a way
				 * is found...), then we mark the entire macro arg as scanned;
				 * however, the two macro arg characters (\1) will be ignored,
				 * so they shouldn't be counted in the scan distance!
				 */
				lexerState->macroArgScanDistance += strlen(str) - 2;

				c = str[0];
			} else {
				c = '\\';
			}
		} else if (c == '{' && !lexerState->disableInterpolation) {
			/* If character is an open brace, do symbol interpolation */
			shiftChars(1);
			char const *ptr = readInterpolation();

			if (ptr) {
				beginExpansion(distance, 0, ptr, strlen(ptr), false, ptr);
				goto restart;
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

	lexerState->colNo += distance;

	if (lexerState->isMmapped) {
		lexerState->offset += distance;
	} else {
		lexerState->index += distance;
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

static void handleCRLF(int c)
{
	if (c == '\r' && peek(0) == '\n')
		shiftChars(1);
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
	lexerState->disableMacroArgs = true;
	lexerState->disableInterpolation = true;
	for (;;) {
		int c = nextChar();

		switch (c) {
		case EOF:
			error("Unterminated block comment\n");
			goto finish;
		case '\r':
			/* Handle CRLF before nextLine() since shiftChars updates colNo */
			handleCRLF(c);
			/* fallthrough */
		case '\n':
			if (!lexerState->expansions || lexerState->expansions->distance)
				nextLine();
			continue;
		case '/':
			if (peek(0) == '*') {
				warning(WARNING_NESTED_COMMENT,
					"/* in block comment\n");
			}
			continue;
		case '*':
			if (peek(0) == '/') {
				shiftChars(1);
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
		int c = peek(0);

		if (c == EOF || c == '\r' || c == '\n')
			break;
		shiftChars(1);
	}
	lexerState->disableMacroArgs = false;
	lexerState->disableInterpolation = false;
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
			/* Handle CRLF before nextLine() since shiftChars updates colNo */
			handleCRLF(c);
			if (!lexerState->expansions || lexerState->expansions->distance)
				nextLine();
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

/* Function to read an anonymous label ref */

static void readAnonLabelRef(char c)
{
	uint32_t n = 0;

	// We come here having already peeked at one char, so no need to do it again
	do {
		shiftChars(1);
		n++;
	} while (peek(0) == c);

	sym_WriteAnonLabelName(yylval.tzSym, n, c == '-');
}

/* Functions to lex numbers of various radixes */

static void readNumber(int radix, int32_t baseValue)
{
	uint32_t value = baseValue;

	for (;; shiftChars(1)) {
		int c = peek(0);

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
	for (;; shiftChars(1)) {
		int c = peek(0);

		if (c == '_')
			continue;
		else if (c < '0' || c > '9')
			break;
		if (divisor > (UINT32_MAX - (c - '0')) / 10) {
			warning(WARNING_LARGE_CONSTANT,
				"Precision of fixed-point constant is too large\n");
			/* Discard any additional digits */
			shiftChars(1);
			while (c = peek(0), (c >= '0' && c <= '9') || c == '_')
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
	uint16_t fractional = (uint16_t)round(value * 65536.0 / divisor);

	yylval.nConstValue |= fractional * (yylval.nConstValue >= 0 ? 1 : -1);
}

char binDigits[2];

static void readBinaryNumber(void)
{
	uint32_t value = 0;

	dbgPrint("Reading binary number with digits [%c,%c]\n", binDigits[0], binDigits[1]);
	for (;; shiftChars(1)) {
		int c = peek(0);
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
	for (;; shiftChars(1)) {
		int c = peek(0);

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

/* Functions to read identifiers & keywords */

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

static char const *readInterpolation(void)
{
	char symName[MAXSYMLEN + 1];
	size_t i = 0;
	struct FormatSpec fmt = fmt_NewSpec();

	for (;;) {
		int c = peek(0);

		if (c == '{') { /* Nested interpolation */
			shiftChars(1);
			char const *ptr = readInterpolation();

			if (ptr) {
				beginExpansion(0, 0, ptr, strlen(ptr), false, ptr);
				continue; /* Restart, reading from the new buffer */
			}
		} else if (c == EOF || c == '\r' || c == '\n' || c == '"') {
			error("Missing }\n");
			break;
		} else if (c == '}') {
			shiftChars(1);
			break;
		} else if (c == ':' && !fmt_IsFinished(&fmt)) { /* Format spec, only once */
			shiftChars(1);
			for (size_t j = 0; j < i; j++)
				fmt_UseCharacter(&fmt, symName[j]);
			fmt_FinishCharacters(&fmt);
			symName[i] = '\0';
			if (!fmt_IsValid(&fmt))
				error("Invalid format spec '%s'\n", symName);
			i = 0; /* Now that format has been set, restart at beginning of string */
		} else {
			shiftChars(1);
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
	if (peek(0) == '"') {
		shiftChars(1);
		if (peek(0) == '"') {
			// """ begins a multi-line string
			shiftChars(1);
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			goto finish;
		}
	}

	for (;;) {
		int c = peek(0);

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			break;
		}

		// We'll be staying in the string, so we can safely consume the char
		shiftChars(1);

		// Handle '\r' or '\n' (in multiline strings only, already handled above otherwise)
		if (c == '\r' || c == '\n') {
			/* Handle CRLF before nextLine() since shiftChars updates colNo */
			handleCRLF(c);
			nextLine();
			c = '\n';
		}

		switch (c) {
		case '"':
			if (multiline) {
				// Only """ ends a multi-line string
				if (peek(0) != '"')
					break;
				shiftChars(1);
				if (peek(0) != '"') {
					append_yylval_tzString('"');
					break;
				}
				shiftChars(1);
			}
			goto finish;

		case '\\': // Character escape or macro arg
			c = peek(0);
			switch (c) {
			case '\\':
			case '"':
			case '{':
			case '}':
				// Return that character unchanged
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
				shiftChars(1);
				char const *str = readMacroArg(c);

				while (*str)
					append_yylval_tzString(*str++);
				continue; // Do not copy an additional character

			case EOF: // Can't really print that one
				error("Illegal character escape at end of input\n");
				c = '\\';
				break;

			default:
				error("Illegal character escape '%s'\n", print(c));
				shiftChars(1);
				break;
			}
			break;

		case '{': // Symbol interpolation
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			char const *ptr = readInterpolation();

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
	if (peek(0) == '"') {
		append_yylval_tzString('"');
		shiftChars(1);
		if (peek(0) == '"') {
			// """ begins a multi-line string
			append_yylval_tzString('"');
			shiftChars(1);
			multiline = true;
		} else {
			// "" is an empty string, skip the loop
			goto finish;
		}
	}

	for (;;) {
		int c = peek(0);

		// '\r', '\n' or EOF ends a single-line string early
		if (c == EOF || (!multiline && (c == '\r' || c == '\n'))) {
			error("Unterminated string\n");
			break;
		}

		// We'll be staying in the string, so we can safely consume the char
		shiftChars(1);

		// Handle '\r' or '\n' (in multiline strings only, already handled above otherwise)
		if (c == '\r' || c == '\n') {
			/* Handle CRLF before nextLine() since shiftChars updates colNo */
			handleCRLF(c);
			nextLine();
			c = '\n';
		}

		switch (c) {
		case '"':
			if (multiline) {
				// Only """ ends a multi-line string
				if (peek(0) != '"')
					break;
				append_yylval_tzString('"');
				shiftChars(1);
				if (peek(0) != '"')
					break;
				append_yylval_tzString('"');
				shiftChars(1);
			}
			append_yylval_tzString('"');
			goto finish;

		case '\\': // Character escape or macro arg
			c = peek(0);
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
				shiftChars(1);
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
				shiftChars(1);
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
				shiftChars(1);
				break;

			default:
				error("Illegal character escape '%s'\n", print(c));
				shiftChars(1);
				break;
			}
			break;

		case '{': // Symbol interpolation
			// We'll be exiting the string scope, so re-enable expansions
			// (Not interpolations, since they're handled by the function itself...)
			lexerState->disableMacroArgs = false;
			char const *ptr = readInterpolation();

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
			return T_TOKEN_AT;

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

		case '\'':
			return T_PRIME;
		case '?':
			return T_QUESTION;

		/* Handle ambiguous 1- or 2-char tokens */

		case '*': /* Either MUL or EXP */
			if (peek(0) == '*') {
				shiftChars(1);
				return T_OP_EXP;
			}
			return T_OP_MUL;

		case '/': /* Either division or a block comment */
			if (peek(0) == '*') {
				shiftChars(1);
				discardBlockComment();
				break;
			}
			return T_OP_DIV;

		case '|': /* Either binary or logical OR */
			if (peek(0) == '|') {
				shiftChars(1);
				return T_OP_LOGICOR;
			}
			return T_OP_OR;

		case '=': /* Either SET alias, or EQ */
			if (peek(0) == '=') {
				shiftChars(1);
				return T_OP_LOGICEQU;
			}
			return T_POP_EQUAL;

		case '<': /* Either a LT, LTE, or left shift */
			switch (peek(0)) {
			case '=':
				shiftChars(1);
				return T_OP_LOGICLE;
			case '<':
				shiftChars(1);
				return T_OP_SHL;
			default:
				return T_OP_LOGICLT;
			}

		case '>': /* Either a GT, GTE, or right shift */
			switch (peek(0)) {
			case '=':
				shiftChars(1);
				return T_OP_LOGICGE;
			case '>':
				shiftChars(1);
				if (peek(0) == '>') {
					shiftChars(1);
					return T_OP_SHRL;
				}
				return T_OP_SHR;
			default:
				return T_OP_LOGICGT;
			}

		case '!': /* Either a NEQ, or negation */
			if (peek(0) == '=') {
				shiftChars(1);
				return T_OP_LOGICNE;
			}
			return T_OP_LOGICNOT;

		/* Handle colon, which may begin an anonymous label ref */

		case ':':
			c = peek(0);
			if (c != '+' && c != '-')
				return T_COLON;

			readAnonLabelRef(c);
			return T_ANON;

		/* Handle numbers */

		case '$':
			yylval.nConstValue = 0;
			readHexNumber();
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

		/* Handle 8-bit registers followed by a period */

		case 'A':
		case 'a':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_A;
			}
			goto normal_identifier;

		case 'F':
		case 'f':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_F;
			}
			goto normal_identifier;

		case 'B':
		case 'b':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_B;
			}
			goto normal_identifier;

		case 'C':
		case 'c':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_C;
			}
			goto normal_identifier;

		case 'D':
		case 'd':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_D;
			}
			goto normal_identifier;

		case 'E':
		case 'e':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_E;
			}
			goto normal_identifier;

		case 'H':
		case 'h':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_H;
			}
			goto normal_identifier;

		case 'L':
		case 'l':
			if (peek(0) == '.') {
				shiftChars(1);
				lexerState->nextToken = T_PERIOD;
				return T_TOKEN_L;
			}
			/* fallthrough */

		/* Handle identifiers... or report garbage characters */

normal_identifier:
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

						beginExpansion(0, 0, s, strlen(s), false,
							       sym->name);
						continue; /* Restart, reading from the new buffer */
					}
				}

				if (tokenType == T_ID && (lexerState->atLineStart || peek(0) == ':'))
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

	/* This is essentially a modified `appendStringLiteral` */
	size_t i = 0;
	int c;

	/* Trim left whitespace (stops at a block comment or line continuation) */
	while (isWhitespace(peek(0)))
		shiftChars(1);

	for (;;) {
		c = peek(0);

		switch (c) {
		case '"': /* String literals inside macro args */
			shiftChars(1);
			i = appendStringLiteral(i);
			break;

		case ';': /* Comments inside macro args */
			discardComment();
			c = peek(0);
			/* fallthrough */
		case ',': /* End of macro arg */
		case '\r':
		case '\n':
		case EOF:
			goto finish;

		case '/': /* Block comments inside macro args */
			shiftChars(1); /* Shift the slash */
			if (peek(0) == '*') {
				shiftChars(1);
				discardBlockComment();
				continue;
			}
			append_yylval_tzString(c); /* Append the slash */
			break;

		case '\\': /* Character escape */
			shiftChars(1); /* Shift the backslash */
			c = peek(0);

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
				error("Illegal character escape '%s'\n", print(c));
				break;
			}
			/* fallthrough */

		default: /* Regular characters will just get copied */
			append_yylval_tzString(c);
			shiftChars(1);
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
		shiftChars(1);
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
		shiftChars(1);
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
				/* Handle CRLF before nextLine() since shiftChars updates colNo */
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
				c = peek(0);
				if (!isWhitespace(c))
					break;
				shiftChars(1);
			}

			if (startsIdentifier(c)) {
				shiftChars(1);
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
				/* Handle CRLF before nextLine() since shiftChars updates colNo */
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
		if (!lexerState->expansions || lexerState->expansions->distance)
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
