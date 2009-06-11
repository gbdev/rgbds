/*	asm.h
 *
 *	Contains some assembler-wide defines and externs
 *
 *	Copyright 1997 Carsten Sorensen
 *
 */

#ifndef ASM_H
#define ASM_H

#include	<stdlib.h>
#include	<stdio.h>
#include	"types.h"
#include	"symbol.h"
#include	"localasm.h"
#include	"asmotor.h"

extern	SLONG	nLineNo;
extern	ULONG	nTotalLines;
extern	ULONG	nPC;
extern	ULONG	nPass;
extern	ULONG	nIFDepth;
extern	char	tzCurrentFileName[_MAX_PATH+1];
extern	struct	Section	*pCurrentSection;
extern	struct	sSymbol	*tHashedSymbols[HASHSIZE];
extern	struct	sSymbol	*pPCSymbol;
extern	UBYTE	oDontExpandStrings;

#define MAXMACROARGS	9
#define MAXINCPATHS		16

#endif	//	ASM_H