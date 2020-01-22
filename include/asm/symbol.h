/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_SYMBOL_H
#define RGBDS_SYMBOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "types.h"

#define HASHSIZE	(1 << 16)
#define MAXSYMLEN	256

enum SymbolType {
	SYM_LABEL,
	SYM_EQU,
	SYM_SET,
	SYM_MACRO,
	SYM_EQUS,
	SYM_REF // Forward reference to a label
};

struct sSymbol {
	char tzName[MAXSYMLEN + 1];
	enum SymbolType type;
	bool isConstant; /* Whether the symbol's value is currently known */
	bool isExported; /* Whether the symbol is to be exported */
	struct sSymbol *pScope;
	struct sSymbol *pNext;
	struct Section *pSection;
	int32_t nValue;
	uint32_t ulMacroSize;
	char *pMacro;
	int32_t (*Callback)(struct sSymbol *self);
	char tzFileName[_MAX_PATH + 1]; /* File where the symbol was defined. */
	uint32_t nFileLine; /* Line where the symbol was defined. */
};

static inline bool sym_IsDefined(struct sSymbol const *sym)
{
	return sym->type != SYM_REF;
}
static inline bool sym_IsConstant(struct sSymbol const *sym)
{
	return sym->isConstant;
}
static inline bool sym_IsNumeric(struct sSymbol const *sym)
{
	return sym->type == SYM_LABEL || sym->type == SYM_EQU
	    || sym->type == SYM_SET;
}
static inline bool sym_IsLocal(struct sSymbol const *sym)
{
	return (sym->type == SYM_LABEL || sym->type == SYM_REF)
		&& strchr(sym->tzName, '.');
}
static inline bool sym_IsExported(struct sSymbol const *sym)
{
	return sym->isExported;
}
/* Symbol will be relocated during linking, it's absolute value is unknown
#define SYMF_RELOC	0x001
Symbol is defined using EQU, will not be changed during linking
#define SYMF_EQU	0x002
Symbol is (re)defined using SET, will not be changed during linking
#define SYMF_SET	0x004
Symbol should be exported
#define SYMF_EXPORT	0x008
Symbol referenced in RPN expression
#define SYMF_REF	0x010
Symbol is a local symbol
#define SYMF_LOCAL	0x020
Symbol has been defined, not only referenced
#define SYMF_DEFINED	0x040
Symbol is a macro
#define SYMF_MACRO	0x080
Symbol is a stringsymbol
#define SYMF_STRING	0x100
Symbol has a constant value, will not be changed during linking
#define SYMF_CONST	0x200
*/

uint32_t sym_CalcHash(const char *s);
void sym_SetExportAll(uint8_t set);
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
void sym_AddEqu(char *tzSym, int32_t value);
void sym_AddSet(char *tzSym, int32_t value);
void sym_Init(void);
uint32_t sym_GetConstantValue(char *s);
struct sSymbol *sym_FindSymbol(char *tzName);
char *sym_FindMacroArg(int32_t i);
char *sym_GetStringValue(struct sSymbol const *sym);
void sym_UseCurrentMacroArgs(void);
void sym_SetMacroArgID(uint32_t nMacroCount);
void sym_AddMacro(char *tzSym, int32_t nDefLineNo);
void sym_Ref(char *tzSym);
void sym_ShiftCurrentMacroArgs(void);
void sym_AddString(char *tzSym, char *tzValue);
uint32_t sym_GetDefinedValue(char *s);
void sym_Purge(char *tzName);
bool sym_IsRelocDiffDefined(char *tzSym1, char *tzSym2);

/* Functions to save and restore the current symbol scope. */
struct sSymbol *sym_GetCurrentSymbolScope(void);
void sym_SetCurrentSymbolScope(struct sSymbol *pNewScope);

#endif /* RGBDS_SYMBOL_H */
