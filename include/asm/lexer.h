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
struct LexerState *lexer_OpenFileView(char *buf, size_t size, uint32_t lineNo);
void lexer_RestartRept(uint32_t lineNo);
void lexer_DeleteState(struct LexerState *state);
void lexer_Init(void);

enum LexerMode {
	LEXER_NORMAL,
	LEXER_RAW,
	LEXER_SKIP_TO_ELIF,
	LEXER_SKIP_TO_ENDC
};

void lexer_SetMode(enum LexerMode mode);
void lexer_ToggleStringExpansion(bool enable);

char const *lexer_GetFileName(void);
uint32_t lexer_GetLineNo(void);
uint32_t lexer_GetColNo(void);
void lexer_DumpStringExpansions(void);
int yylex(void);
void lexer_CaptureBlock(int blockStartToken, int blockEndToken, char **capture, size_t *size,
			char const *name);

#endif /* RGBDS_ASM_LEXER_H */
