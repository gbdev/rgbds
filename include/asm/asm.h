/*	asm.h
 *
 *	Contains some assembler-wide defines and externs
 *
 *	Copyright 1997 Carsten Sorensen
 *
 */

#ifndef RGBDS_ASM_ASM_H
#define RGBDS_ASM_ASM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "asm/symbol.h"

#include "asm/localasm.h"

extern SLONG nLineNo;
extern ULONG nTotalLines;
extern ULONG nPC;
extern ULONG nPass;
extern ULONG nIFDepth;
extern char tzCurrentFileName[_MAX_PATH + 1];
extern struct Section *pCurrentSection;
extern struct sSymbol *tHashedSymbols[HASHSIZE];
extern struct sSymbol *pPCSymbol;
extern bool oDontExpandStrings;

#define MAXMACROARGS	256
#define MAXINCPATHS		128

#endif	/* //       ASM_H */
