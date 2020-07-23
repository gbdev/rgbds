/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_LEXER_H
#define RGBDS_ASM_LEXER_H

#define MAXSTRLEN	255

struct LexerState;
extern struct LexerState *lexerState;
extern struct LexerState *lexerStateEOL;

static inline struct LexerState *lexer_GetState(void)
{
	return lexerState;
}

static inline void lexer_SetState(struct LexerState *state)
{
	lexerState = state;
}

static inline void lexer_SetStateAtEOL(struct LexerState *state)
{
	lexerStateEOL = state;
}

struct LexerState *lexer_OpenFile(char const *path);
struct LexerState *lexer_OpenFileView(void);
void lexer_DeleteState(struct LexerState *state);

enum LexerMode {
	LEXER_NORMAL,
	LEXER_RAW
};

void lexer_SetMode(enum LexerMode mode);
void lexer_ToggleStringExpansion(bool enable);

char const *lexer_GetFileName(void);
unsigned int lexer_GetLineNo(void);
void lexer_DumpStringExpansions(void);
int yylex(void);
void lexer_SkipToBlockEnd(int blockStartToken, int blockEndToken, int endToken,
			  char **capture, size_t *size, char const *name);

#endif /* RGBDS_ASM_LEXER_H */
