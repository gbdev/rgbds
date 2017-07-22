/*	fstack.h
 *
 *	Contains some assembler-wide defines and externs
 *
 *	Copyright 1997 Carsten Sorensen
 *
 */

#ifndef RGBDS_ASM_FSTACK_H
#define RGBDS_ASM_FSTACK_H

#include <stdio.h>

#include "asm/asm.h"
#include "types.h"
#include "asm/lexer.h"

struct sContext {
	YY_BUFFER_STATE FlexHandle;
	struct sSymbol *pMacro;
	struct sContext *pNext;
	char tzFileName[_MAX_PATH + 1];
	char *tzMacroArgs[MAXMACROARGS + 1];
	SLONG nLine;
	ULONG nStatus;
	FILE *pFile;
	char *pREPTBlock;
	ULONG nREPTBlockCount;
	ULONG nREPTBlockSize;
};

void
fstk_RunInclude(char *);
extern void fstk_RunMacroArg(SLONG s);
void
fstk_Init(char *);
extern void fstk_Dump(void);
extern void fstk_AddIncludePath(char *s);
extern ULONG fstk_RunMacro(char *s);
extern void fstk_RunRept(ULONG count);
FILE *
fstk_FindFile(char *);

int fstk_GetLine(void);

extern int yywrap(void);

#endif
