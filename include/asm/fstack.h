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

struct sContext {
	YY_BUFFER_STATE FlexHandle;
	struct sSymbol *pMacro;
	struct sContext *pNext;
	char tzFileName[_MAX_PATH + 1];
	char *tzMacroArgs[MAXMACROARGS + 1];
	int32_t nLine;
	uint32_t nStatus;
	FILE *pFile;
	char *pREPTBlock;
	uint32_t nREPTBlockCount;
	uint32_t nREPTBlockSize;
	int32_t nREPTBodyFirstLine;
	int32_t nREPTBodyLastLine;
};

extern unsigned int nMaxRecursionDepth;

void fstk_RunInclude(char *tzFileName);
void fstk_RunMacroArg(int32_t s);
void fstk_Init(char *s);
void fstk_Dump(void);
void fstk_DumpToStr(char *buf, size_t len);
void fstk_DumpStringExpansions(void);
void fstk_AddIncludePath(char *s);
uint32_t fstk_RunMacro(char *s);
void fstk_RunRept(uint32_t count, int32_t nReptLineNo);
FILE *fstk_FindFile(char const *fname, char **incPathUsed);
int32_t fstk_GetLine(void);

#endif /* RGBDS_ASM_FSTACK_H */
