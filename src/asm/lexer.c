/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asm/lexer.h"
#include "asm/rpn.h"
#include "asm/symbol.h" /* For MAXSYMLEN in asmy.h */
#include "asm/warning.h"
/* Include this last so it gets all type & constant definitions */
#include "asmy.h" /* For token definitions, generated from asmy.y */

#define LEXER_BUF_SIZE 42 /* TODO: determine a sane value for this */
/* This caps the size of buffer reads, and according to POSIX, passing more than SSIZE_MAX is UB */
static_assert(LEXER_BUF_SIZE <= SSIZE_MAX);

struct LexerState {
	char const *path;

	/* mmap()-dependent IO state */
	bool isMmapped;
	union {
		struct { /* If mmap()ed */
			char *ptr;
			off_t size;
			off_t offset;
		};
		struct { /* Otherwise */
			int fd;
			size_t index; /* Read index into the buffer */
			size_t nbChars; /* Number of chars in front of the buffer */
			char buf[LEXER_BUF_SIZE]; /* Circular buffer */
		};
	};

	/* Common state */
	enum LexerMode mode;
	bool atLineStart;
	unsigned int lineNo;
	bool capturing; /* Whether the text being lexed should be captured */
	size_t captureSize; /* Amount of text captured */
	char *captureBuf; /* Buffer to send the captured text to if non-NULL */
	size_t captureCapacity; /* Size of the buffer above */
	bool expandStrings;
};

struct LexerState *lexerState = NULL;
struct LexerState *lexerStateEOL = NULL;

struct LexerState *lexer_OpenFile(char const *path)
{
	bool isStdin = !strcmp(path, "-");
	struct LexerState *state = malloc(sizeof(*state));

	/* Give stdin a nicer file name */
	if (isStdin)
		path = "<stdin>";
	if (!state) {
		error("Failed to open file \"%s\": %s\n", path, strerror(errno));
		return NULL;
	}
	state->path = path;

	state->fd = isStdin ? STDIN_FILENO : open(path, O_RDONLY);
	state->isMmapped = false; /* By default, assume it won't be mmap()ed */
	off_t size = lseek(state->fd, 0, SEEK_END);

	if (size != 1) {
		/* The file is a regular file, so use `mmap` for better performance */

		/*
		 * Important: do NOT assign to `state->ptr` directly, to avoid a cast that may
		 * alter an eventual `MAP_FAILED` value. It would also invalidate `state->fd`,
		 * being on the other side of the union.
		 */
		void *pa = mmap(NULL, size, PROT_READ, MAP_PRIVATE, state->fd, 0);

		if (pa == MAP_FAILED && errno == ENOTSUP)
			/*
			 * The implementation may not support MAP_PRIVATE; try again with MAP_SHARED
			 * instead, offering, I believe, weaker guarantees about external
			 * modifications to the file while reading it. That's still better than not
			 * opening it at all, though.
			 */
			pa = mmap(NULL, size, PROT_READ, MAP_SHARED, state->fd, 0);

		if (pa == MAP_FAILED) {
			/* If mmap()ing failed, try again using another method (below) */
			state->isMmapped = false;
		} else {
			/* IMPORTANT: the `union` mandates this is accessed before other members! */
			close(state->fd);

			state->isMmapped = true;
			state->ptr = pa;
			state->size = size;
		}
	}
	if (!state->isMmapped) {
		/* Sometimes mmap() fails or isn't available, so have a fallback */
		lseek(state->fd, 0, SEEK_SET);
		state->index = 0;
		state->nbChars = 0;
	}

	state->mode = LEXER_NORMAL;
	state->atLineStart = true;
	state->lineNo = 0;
	state->capturing = false;
	state->captureBuf = NULL;
	return state;
}

struct LexerState *lexer_OpenFileView(void)
{
	return NULL;
}

void lexer_DeleteState(struct LexerState *state)
{
	if (state->isMmapped)
		munmap(state->ptr, state->size);
	else
		close(state->fd);
	free(state);
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
	lexerState->captureCapacity *= 2;
	lexerState->captureBuf = realloc(lexerState->captureBuf, lexerState->captureCapacity);
	if (!lexerState->captureBuf)
		fatalerror("realloc error while resizing capture buffer: %s\n", strerror(errno));
}

/* If at any point we need more than 255 characters of lookahead, something went VERY wrong. */
static int peek(uint8_t distance)
{
	if (lexerState->isMmapped) {
		if (lexerState->offset + distance >= lexerState->size)
			return EOF;
		return lexerState->ptr[lexerState->offset + distance];
	}

	if (lexerState->nbChars <= distance) {
		/* Buffer isn't full enough, read some chars in */

		/* Compute the index we'll start writing to */
		size_t writeIndex = (lexerState->index + lexerState->nbChars) % LEXER_BUF_SIZE;
		size_t target = LEXER_BUF_SIZE - lexerState->nbChars; /* Aim: making the buf full */
		ssize_t nbCharsRead = 0;

#define readChars(size) do { \
	nbCharsRead = read(lexerState->fd, &lexerState->buf[writeIndex], (size)); \
	if (nbCharsRead == -1) \
		fatalerror("Error while reading \"%s\": %s\n", lexerState->path, errno); \
	writeIndex += nbCharsRead; \
	if (writeIndex == LEXER_BUF_SIZE) \
		writeIndex = 0; \
	lexerState->nbChars += nbCharsRead; /* Count all those chars in */ \
	target -= nbCharsRead; \
} while (0)

		/* If the range to fill passes over the buffer wrapping point, we need two reads */
		if (writeIndex + target > LEXER_BUF_SIZE) {
			readChars(LEXER_BUF_SIZE - writeIndex);
			/* If the read was incomplete, don't perform a second read */
			if (nbCharsRead < LEXER_BUF_SIZE - writeIndex)
				target = 0;
		}
		if (target != 0)
			readChars(target);

#undef readChars

		/* If there aren't enough chars even after refilling, give up */
		if (lexerState->nbChars <= distance)
			return EOF;
	}
	return lexerState->buf[(lexerState->index + distance) % LEXER_BUF_SIZE];
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

	if (lexerState->isMmapped) {
		lexerState->offset += distance;
	} else {
		lexerState->nbChars -= distance;
		lexerState->index += distance;
		/* Wrap around if necessary */
		if (lexerState->index >= LEXER_BUF_SIZE)
			lexerState->index %= LEXER_BUF_SIZE;
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
	return lexerState->path;
}

unsigned int lexer_GetLineNo(void)
{
	return lexerState->lineNo;
}

void lexer_DumpStringExpansions(void)
{
	/* TODO */
}

static int yylex_NORMAL(void)
{
	for (;;) {
		int c = nextChar();

		switch (c) {
		case '\n':
			if (lexerStateEOL) {
				lexer_SetState(lexerStateEOL);
				lexerStateEOL = NULL;
			}
			return '\n';

		/* Ignore whitespace */
		case ' ':
		case '\t':
			break;

		case EOF:
			/* Captures end at their buffer's boundary no matter what */
			if (!lexerState->capturing) {
				/* TODO: use `yywrap()` */
			}
			return 0;

		default:
			error("Unknown character '%c'\n");
		}
	}
}

static int yylex_RAW(void)
{
	fatalerror("LEXER_RAW not yet implemented\n");
}

int yylex(void)
{
	if (lexerState->atLineStart)
		lexerState->lineNo++;

	static int (* const lexerModeFuncs[])(void) = {
		[LEXER_NORMAL] = yylex_NORMAL,
		[LEXER_RAW]    = yylex_RAW,
	};
	int token = lexerModeFuncs[lexerState->mode]();

	if (token == '\n')
		lexerState->atLineStart = true;
	else if (lexerState->atLineStart)
		lexerState->atLineStart = false;

	return token;
}

void lexer_SkipToBlockEnd(int blockStartToken, int blockEndToken, int endToken,
			  char **capture, size_t *size, char const *name)
{
	lexerState->capturing = true;
	lexerState->captureSize = 0;
	unsigned int level = 0;
	char *captureStart;

	if (capture) {
		if (lexerState->isMmapped) {
			captureStart = lexerState->ptr;
		} else {
			lexerState->captureCapacity = 128; /* The initial size will be twice that */
			reallocCaptureBuf();
			captureStart = lexerState->captureBuf;
		}
	}

	for (;;) {
		int token = yylex();

		if (level == 0) {
			if (token == endToken)
				break;
			/*
			 * Hack: skipping after a `if` requires stopping on three different tokens,
			 * which there is no simple way to make this function support. Instead,
			 * if ELIF is the end token, ELSE and ENDC are also checked for here.
			 */
			if (endToken == T_POP_ELIF && (token == T_POP_ELSE || token == T_POP_ENDC))
				break;
		}

		if (token == EOF)
			error("Unterminated %s\n", name);
		else if (token == blockStartToken)
			level++;
		else if (token == blockEndToken)
			level--;
	}

	if (capture) {
		*capture = captureStart;
		*size = lexerState->captureSize;
	}
	lexerState->captureBuf = NULL;
}
