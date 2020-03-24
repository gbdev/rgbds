/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2020, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_SYMBOL_H
#define RGBDS_SYMBOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "asm/section.h"

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
	bool isExported; /* Whether the symbol is to be exported */
	bool isBuiltin;  /* Whether the symbol is a built-in */
	struct sSymbol *pScope;
	struct Section *pSection;
	int32_t nValue;
	uint32_t ulMacroSize;
	char *pMacro;
	int32_t (*Callback)(struct sSymbol const *self);
	char tzFileName[_MAX_PATH + 1]; /* File where the symbol was defined. */
	uint32_t nFileLine; /* Line where the symbol was defined. */

	uint32_t ID; /* ID of the symbol in the object file (-1 if none) */
	struct sSymbol *next; /* Next object to output in the object file */
};

static inline bool sym_IsDefined(struct sSymbol const *sym)
{
	return sym->type != SYM_REF;
}

static inline bool sym_IsConstant(struct sSymbol const *sym)
{
	return sym->type == SYM_EQU || sym->type == SYM_SET
				|| (sym->type == SYM_LABEL && sym->pSection
				    && sym->pSection->nOrg != -1);
}

static inline bool sym_IsNumeric(struct sSymbol const *sym)
{
	return sym->type == SYM_LABEL || sym->type == SYM_EQU
	    || sym->type == SYM_SET;
}

static inline bool sym_IsLabel(struct sSymbol const *sym)
{
	return sym->type == SYM_LABEL || sym->type == SYM_REF;
}

static inline bool sym_IsLocal(struct sSymbol const *sym)
{
	return sym_IsLabel(sym) && strchr(sym->tzName, '.');
}

static inline bool sym_IsExported(struct sSymbol const *sym)
{
	return sym->isExported;
}

/*
 * Get a string equate's value
 */
static inline char *sym_GetStringValue(struct sSymbol const *sym)
{
	return sym->pMacro;
}

void sym_ForEach(void (*func)(struct sSymbol *, void *), void *arg);

int32_t sym_GetValue(struct sSymbol const *sym);
void sym_SetExportAll(bool set);
struct sSymbol *sym_AddLocalReloc(char const *tzSym);
struct sSymbol *sym_AddReloc(char const *tzSym);
void sym_Export(char const *tzSym);
struct sSymbol *sym_FindMacro(char const *s);
struct sSymbol *sym_AddEqu(char const *tzSym, int32_t value);
struct sSymbol *sym_AddSet(char const *tzSym, int32_t value);
void sym_Init(void);
uint32_t sym_GetConstantValue(char const *s);
struct sSymbol *sym_FindSymbol(char const *tzName);
char *sym_GetStringValue(struct sSymbol const *sym);
struct sSymbol *sym_AddMacro(char const *tzSym, int32_t nDefLineNo);
struct sSymbol *sym_Ref(char const *tzSym);
struct sSymbol *sym_AddString(char const *tzSym, char const *tzValue);
uint32_t sym_GetDefinedValue(char const *s);
void sym_Purge(char const *tzName);

/* Functions to save and restore the current symbol scope. */
struct sSymbol *sym_GetCurrentSymbolScope(void);
void sym_SetCurrentSymbolScope(struct sSymbol *pNewScope);

#endif /* RGBDS_SYMBOL_H */
