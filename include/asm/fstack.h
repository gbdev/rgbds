/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Contains some assembler-wide defines and externs
 */

#ifndef RGBDS_ASM_FSTACK_H
#define RGBDS_ASM_FSTACK_H

#include <stdint.h>
#include <stdio.h>

#include "asm/asm.h"
#include "asm/lexer.h"

#include "types.h"

struct MacroArgs;

struct sContext {
	struct LexerState *lexerState;
	struct Symbol const *pMacro;
	struct sContext *next;
	char tzFileName[_MAX_PATH + 1];
	struct MacroArgs *macroArgs;
	uint32_t uniqueID;
	int32_t nLine;
	uint32_t nStatus;
	char const *pREPTBlock;
	uint32_t nREPTBlockCount;
	uint32_t nREPTBlockSize;
	int32_t nREPTBodyFirstLine;
	int32_t nREPTBodyLastLine;
};

extern unsigned int nMaxRecursionDepth;

void fstk_RunInclude(char *tzFileName);
void fstk_Init(char *s);
void fstk_Dump(void);
void fstk_DumpToStr(char *buf, size_t len);
void fstk_AddIncludePath(char *s);
void fstk_RunMacro(char *s, struct MacroArgs *args);
void fstk_RunRept(uint32_t count, int32_t nReptLineNo, char const *body, size_t size);
/**
 * @param path The user-provided file name
 * @param fullPath The address of a pointer, which will be made to point at the full path
 *                 The pointer's value must be a valid argument to `realloc`, including NULL
 * @param size Current size of the buffer, or 0 if the pointer is NULL
 * @return True if the file was found, false if no path worked
 */
bool fstk_FindFile(char const *path, char **fullPath, size_t *size);
int32_t fstk_GetLine(void);

#endif /* RGBDS_ASM_FSTACK_H */
