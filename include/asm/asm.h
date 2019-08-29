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

#ifndef RGBDS_ASM_ASM_H
#define RGBDS_ASM_ASM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "asm/localasm.h"
#include "asm/symbol.h"

#define MAXUNIONS	128
#define MAXMACROARGS	256
#define MAXINCPATHS	128

extern int32_t nLineNo;
extern uint32_t nTotalLines;
extern uint32_t nPC;
extern uint32_t nIFDepth;
extern bool skipElif;
extern uint32_t nUnionDepth;
extern uint32_t unionStart[MAXUNIONS];
extern uint32_t unionSize[MAXUNIONS];
extern char tzCurrentFileName[_MAX_PATH + 1];
extern struct Section *pCurrentSection;
extern struct sSymbol *tHashedSymbols[HASHSIZE];
extern struct sSymbol *pPCSymbol;
extern bool oDontExpandStrings;

size_t symvaluetostring(char *dest, size_t maxLength, char *sym,
			const char *mode);

#endif /* RGBDS_ASM_ASM_H */
