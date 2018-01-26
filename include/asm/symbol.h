/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_SYMBOL_H
#define RGBDS_SYMBOL_H

#include <stdint.h>

#include "types.h"

#define HASHSIZE	(1 << 16)
#define MAXSYMLEN	256

struct sSymbol {
	char tzName[MAXSYMLEN + 1];
	int32_t nValue;
	uint32_t nType;
	struct sSymbol *pScope;
	struct sSymbol *pNext;
	struct Section *pSection;
	uint32_t ulMacroSize;
	char *pMacro;
	int32_t (*Callback)(struct sSymbol *);
	char tzFileName[_MAX_PATH + 1]; /* File where the symbol was defined. */
	uint32_t nFileLine; /* Line where the symbol was defined. */
};

/* Symbol will be relocated during linking, it's absolute value is unknown */
#define SYMF_RELOC	0x001
/* Symbol is defined using EQU, will not be changed during linking */
#define SYMF_EQU	0x002
/* Symbol is (re)defined using SET, will not be changed during linking */
#define SYMF_SET	0x004
/* Symbol should be exported */
#define SYMF_EXPORT	0x008
/* Symbol is imported, it's value is unknown */
#define SYMF_IMPORT	0x010
/* Symbol is a local symbol */
#define SYMF_LOCAL	0x020
/* Symbol has been defined, not only referenced */
#define SYMF_DEFINED	0x040
/* Symbol is a macro */
#define SYMF_MACRO	0x080
/* Symbol is a stringsymbol */
#define SYMF_STRING	0x100
/* Symbol has a constant value, will not be changed during linking */
#define SYMF_CONST	0x200

uint32_t calchash(char *s);
void sym_SetExportAll(uint8_t set);
void sym_PrepPass1(void);
void sym_PrepPass2(void);
void sym_AddLocalReloc(char *tzSym);
void sym_AddReloc(char *tzSym);
void sym_Export(char *tzSym);
void sym_PrintSymbolTable(void);
struct sSymbol *sym_FindMacro(char *s);
void sym_InitNewMacroArgs(void);
void sym_AddNewMacroArg(char *s);
void sym_SaveCurrentMacroArgs(char *save[]);
void sym_RestoreCurrentMacroArgs(char *save[]);
void sym_UseNewMacroArgs(void);
void sym_FreeCurrentMacroArgs(void);
void sym_AddEqu(char *tzSym, int32_t value);
void sym_AddSet(char *tzSym, int32_t value);
void sym_Init(void);
uint32_t sym_GetConstantValue(char *s);
uint32_t sym_isConstant(char *s);
struct sSymbol *sym_FindSymbol(char *tzName);
void sym_Global(char *tzSym);
char *sym_FindMacroArg(int32_t i);
char *sym_GetStringValue(char *tzSym);
void sym_UseCurrentMacroArgs(void);
void sym_SetMacroArgID(uint32_t nMacroCount);
uint32_t sym_isString(char *tzSym);
void sym_AddMacro(char *tzSym);
void sym_ShiftCurrentMacroArgs(void);
void sym_AddString(char *tzSym, char *tzValue);
uint32_t sym_GetValue(char *s);
uint32_t sym_GetDefinedValue(char *s);
uint32_t sym_isDefined(char *tzName);
void sym_Purge(char *tzName);
uint32_t sym_isConstDefined(char *tzName);
int32_t sym_IsRelocDiffDefined(char *tzSym1, char *tzSym2);

/* Functions to save and restore the current symbol scope. */
struct sSymbol *sym_GetCurrentSymbolScope(void);
void sym_SetCurrentSymbolScope(struct sSymbol *pNewScope);

#endif /* RGBDS_SYMBOL_H */
