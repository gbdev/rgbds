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

#define MAXUNIONS		128
#define MAXMACROARGS	256
#define MAXINCPATHS		128

extern SLONG nLineNo;
extern ULONG nTotalLines;
extern ULONG nPC;
extern ULONG nPass;
extern ULONG nIFDepth;
extern bool skipElif;
extern ULONG nUnionDepth;
extern ULONG unionStart[MAXUNIONS];
extern ULONG unionSize[MAXUNIONS];
extern char tzCurrentFileName[_MAX_PATH + 1];
extern struct Section *pCurrentSection;
extern struct sSymbol *tHashedSymbols[HASHSIZE];
extern struct sSymbol *pPCSymbol;
extern bool oDontExpandStrings;

#endif	/* //       ASM_H */
