/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Symboltable and macroargs stuff
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "asm/asm.h"
#include "asm/fstack.h"
#include "asm/symbol.h"
#include "asm/main.h"
#include "asm/mymath.h"
#include "asm/output.h"

#include "extern/err.h"

#include "helpers.h"
#include "version.h"

struct sSymbol *tHashedSymbols[HASHSIZE];
static struct sSymbol *pScope; /* Current section symbol scope */
struct sSymbol *pPCSymbol;
static struct sSymbol *p_NARGSymbol;
static struct sSymbol *p__LINE__Symbol;
static char *currentmacroargs[MAXMACROARGS + 1];
static char *newmacroargs[MAXMACROARGS + 1];
static char SavedTIME[256];
static char SavedDATE[256];
static char SavedTIMESTAMP_ISO8601_LOCAL[256];
static char SavedTIMESTAMP_ISO8601_UTC[256];
static char SavedDAY[3];
static char SavedMONTH[3];
static char SavedYEAR[20];
static char SavedHOUR[3];
static char SavedMINUTE[3];
static char SavedSECOND[3];
static bool exportall;

void helper_RemoveLeadingZeros(char *string)
{
	char *new_beginning = string;

	while (*new_beginning == '0')
		new_beginning++;

	if (new_beginning == string)
		return;

	if (*new_beginning == '\0')
		new_beginning--;

	memmove(string, new_beginning, strlen(new_beginning) + 1);
}

int32_t Callback_NARG(unused_ struct sSymbol *sym)
{
	uint32_t i = 0;

	while (currentmacroargs[i] && i < MAXMACROARGS)
		i += 1;

	return i;
}

int32_t Callback__LINE__(unused_ struct sSymbol *sym)
{
	return nLineNo;
}

/*
 * Get the nValue field of a symbol
 */
static int32_t getvaluefield(struct sSymbol *sym)
{
	if (sym->Callback)
		return sym->Callback(sym);

	return sym->nValue;
}

/*
 * Calculate the hash value for a string
 */
uint32_t calchash(char *s)
{
	uint32_t hash = 5381;

	while (*s != 0)
		hash = (hash * 33) ^ (*s++);

	return hash % HASHSIZE;
}

/*
 * Create a new symbol by name
 */
struct sSymbol *createsymbol(char *s)
{
	struct sSymbol **ppsym;
	uint32_t hash;

	hash = calchash(s);
	ppsym = &(tHashedSymbols[hash]);

	while ((*ppsym) != NULL)
		ppsym = &((*ppsym)->pNext);

	(*ppsym) = malloc(sizeof(struct sSymbol));

	if ((*ppsym) == NULL) {
		fatalerror("No memory for symbol");
		return NULL;
	}

	if (snprintf((*ppsym)->tzName, MAXSYMLEN + 1, "%s", s) > MAXSYMLEN)
		warning("Symbol name is too long: '%s'", s);

	(*ppsym)->nValue = 0;
	(*ppsym)->nType = 0;
	(*ppsym)->pScope = NULL;
	(*ppsym)->pNext = NULL;
	(*ppsym)->pMacro = NULL;
	(*ppsym)->pSection = NULL;
	(*ppsym)->Callback = NULL;

	if (snprintf((*ppsym)->tzFileName, _MAX_PATH + 1, "%s",
		     tzCurrentFileName) > _MAX_PATH) {
		fatalerror("%s: File name is too long: '%s'", __func__,
			   tzCurrentFileName);
	}

	(*ppsym)->nFileLine = fstk_GetLine();
	return *ppsym;
}

/*
 * Creates the full name of a local symbol in a given scope, by prepending
 * the name with the parent symbol's name.
 */
static size_t fullSymbolName(char *output, size_t outputSize, char *localName,
			     const struct sSymbol *scope)
{
	const struct sSymbol *parent = scope->pScope ? scope->pScope : scope;

	return snprintf(output, outputSize, "%s%s", parent->tzName, localName);
}

/*
 * Find the pointer to a symbol by name and scope
 */
struct sSymbol **findpsymbol(char *s, struct sSymbol *scope)
{
	struct sSymbol **ppsym;
	int32_t hash;
	char fullname[MAXSYMLEN + 1];

	if (s[0] == '.' && scope) {
		fullSymbolName(fullname, sizeof(fullname), s, scope);
		s = fullname;
	}

	char *separator = strchr(s, '.');

	if (separator) {
		if (strchr(separator + 1, '.'))
			fatalerror("'%s' is a nonsensical reference to a nested local symbol",
				   s);
	}

	hash = calchash(s);
	ppsym = &(tHashedSymbols[hash]);

	while ((*ppsym) != NULL) {
		if ((strcmp(s, (*ppsym)->tzName) == 0))
			return ppsym;

		ppsym = &((*ppsym)->pNext);
	}
	return NULL;
}

/*
 * Find a symbol by name and scope
 */
struct sSymbol *findsymbol(char *s, struct sSymbol *scope)
{
	struct sSymbol **ppsym = findpsymbol(s, scope);

	return ppsym ? *ppsym : NULL;
}

/*
 * Find a symbol by name and scope
 */
struct sSymbol *sym_FindSymbol(char *tzName)
{
	struct sSymbol *pscope;

	if (*tzName == '.')
		pscope = pScope;
	else
		pscope = NULL;

	return findsymbol(tzName, pscope);
}

/*
 * Purge a symbol
 */
void sym_Purge(char *tzName)
{
	struct sSymbol **ppSym;
	struct sSymbol *pscope;

	if (*tzName == '.')
		pscope = pScope;
	else
		pscope = NULL;

	ppSym = findpsymbol(tzName, pscope);

	if (ppSym) {
		struct sSymbol *pSym;

		pSym = *ppSym;
		*ppSym = pSym->pNext;

		if (pSym->pMacro)
			free(pSym->pMacro);

		free(pSym);
	} else {
		yyerror("'%s' not defined", tzName);
	}
}

/*
 * Determine if a symbol has been defined
 */
uint32_t sym_isConstDefined(char *tzName)
{
	struct sSymbol *psym, *pscope;

	if (*tzName == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(tzName, pscope);

	if (psym && (psym->nType & SYMF_DEFINED)) {
		uint32_t mask = SYMF_EQU | SYMF_SET | SYMF_MACRO | SYMF_STRING;

		if (psym->nType & mask)
			return 1;

		fatalerror("'%s' is not allowed as argument to the DEF function",
			   tzName);
	}

	return 0;
}

uint32_t sym_isDefined(char *tzName)
{
	struct sSymbol *psym, *pscope;

	if (*tzName == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(tzName, pscope);

	if (psym && (psym->nType & SYMF_DEFINED))
		return 1;
	else
		return 0;
}

/*
 * Determine if the symbol is a constant
 */
uint32_t sym_isConstant(char *s)
{
	struct sSymbol *psym, *pscope;

	if (*s == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(s, pscope);

	if (psym != NULL) {
		if (psym->nType & SYMF_CONST)
			return 1;
	}

	return 0;
}

/*
 * Get a string equate's value
 */
char *sym_GetStringValue(char *tzSym)
{
	const struct sSymbol *pSym = sym_FindSymbol(tzSym);

	if (pSym != NULL)
		return pSym->pMacro;

	yyerror("Stringsymbol '%s' not defined", tzSym);

	return NULL;
}

/*
 * Return a constant symbols value
 */
uint32_t sym_GetConstantValue(char *s)
{
	struct sSymbol *psym, *pscope;

	if (*s == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(s, pscope);

	if (psym != NULL) {
		if (psym->nType & SYMF_CONST)
			return getvaluefield(psym);

		fatalerror("Expression must have a constant value");
	}

	yyerror("'%s' not defined", s);

	return 0;
}

/*
 * Return a symbols value... "estimated" if not defined yet
 */
uint32_t sym_GetValue(char *s)
{
	struct sSymbol *psym, *pscope;

	if (*s == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(s, pscope);

	if (psym != NULL) {
		if (psym->nType & SYMF_DEFINED) {
			if (psym->nType & (SYMF_MACRO | SYMF_STRING))
				yyerror("'%s' is a macro or string symbol", s);

			return getvaluefield(psym);
		}

		if (nPass == 2) {
			/*
			 * Assume undefined symbols are imported from
			 * somwehere else
			 */
			psym->nType |= SYMF_IMPORT;
		}

		/* 0x80 seems like a good default value... */
		return 0x80;
	}

	if (nPass == 1) {
		createsymbol(s);
		return 0x80;
	}

	yyerror("'%s' not defined", s);

	return 0;
}

/*
 * Return a defined symbols value... aborts if not defined yet
 */
uint32_t sym_GetDefinedValue(char *s)
{
	struct sSymbol *psym, *pscope;

	if (*s == '.')
		pscope = pScope;
	else
		pscope = NULL;

	psym = findsymbol(s, pscope);

	if (psym != NULL) {
		if ((psym->nType & SYMF_DEFINED)) {
			if (psym->nType & (SYMF_MACRO | SYMF_STRING))
				yyerror("'%s' is a macro or string symbol", s);

			return getvaluefield(psym);
		}
	}

	yyerror("'%s' not defined", s);

	return 0;
}

struct sSymbol *sym_GetCurrentSymbolScope(void)
{
	return pScope;
}

void sym_SetCurrentSymbolScope(struct sSymbol *pNewScope)
{
	pScope = pNewScope;
}

/*
 * Macro argument stuff
 */
void sym_ShiftCurrentMacroArgs(void)
{
	int32_t i;

	free(currentmacroargs[0]);
	for (i = 0; i < MAXMACROARGS - 1; i += 1)
		currentmacroargs[i] = currentmacroargs[i + 1];

	currentmacroargs[MAXMACROARGS - 1] = NULL;
}

char *sym_FindMacroArg(int32_t i)
{
	if (i == -1)
		i = MAXMACROARGS + 1;

	assert(i - 1 >= 0);

	assert((size_t)(i - 1)
	       < sizeof(currentmacroargs) / sizeof(*currentmacroargs));

	return currentmacroargs[i - 1];
}

void sym_UseNewMacroArgs(void)
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i += 1) {
		currentmacroargs[i] = newmacroargs[i];
		newmacroargs[i] = NULL;
	}
}

void sym_SaveCurrentMacroArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i += 1)
		save[i] = currentmacroargs[i];
}

void sym_RestoreCurrentMacroArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i += 1)
		currentmacroargs[i] = save[i];
}

void sym_FreeCurrentMacroArgs(void)
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i += 1) {
		free(currentmacroargs[i]);
		currentmacroargs[i] = NULL;
	}
}

void sym_AddNewMacroArg(char *s)
{
	int32_t i = 0;

	while (i < MAXMACROARGS && newmacroargs[i] != NULL)
		i += 1;

	if (i < MAXMACROARGS) {
		if (s)
			newmacroargs[i] = strdup(s);
		else
			newmacroargs[i] = NULL;
	} else {
		yyerror("A maximum of %d arguments allowed", MAXMACROARGS);
	}
}

void sym_SetMacroArgID(uint32_t nMacroCount)
{
	char s[256];

	snprintf(s, sizeof(s), "_%u", nMacroCount);
	newmacroargs[MAXMACROARGS] = strdup(s);
}

void sym_UseCurrentMacroArgs(void)
{
	int32_t i;

	for (i = 1; i <= MAXMACROARGS; i += 1)
		sym_AddNewMacroArg(sym_FindMacroArg(i));
}

/*
 * Find a macro by name
 */
struct sSymbol *sym_FindMacro(char *s)
{
	return findsymbol(s, NULL);
}

/*
 * Add an equated symbol
 */
void sym_AddEqu(char *tzSym, int32_t value)
{
	if ((nPass == 1) || ((nPass == 2) && (sym_isDefined(tzSym) == 0))) {
		/* only add equated symbols in pass 1 */
		struct sSymbol *nsym = findsymbol(tzSym, NULL);

		if (nsym != NULL) {
			if (nsym->nType & SYMF_DEFINED) {
				yyerror("'%s' already defined in %s(%d)", tzSym,
					nsym->tzFileName, nsym->nFileLine);
			}
		} else {
			nsym = createsymbol(tzSym);
		}

		if (nsym) {
			nsym->nValue = value;
			nsym->nType |= SYMF_EQU | SYMF_DEFINED | SYMF_CONST;
			nsym->pScope = NULL;
		}
	}
}

/*
 * Add a string equated symbol.
 *
 * If the desired symbol is a string it needs to be passed to this function with
 * quotes inside the string, like sym_AddString("name", "\"test\"), or the
 * assembler won't be able to use it with DB and similar. This is equivalent to
 * ``` name EQUS "\"test\"" ```
 *
 * If the desired symbol is a register or a number, just the terminator quotes
 * of the string are enough: sym_AddString("M_PI", "3.1415"). This is the same
 * as ``` M_PI EQUS "3.1415" ```
 */
void sym_AddString(char *tzSym, char *tzValue)
{
	struct sSymbol *nsym = findsymbol(tzSym, NULL);

	if (nsym != NULL) {
		if (nsym->nType & SYMF_DEFINED) {
			yyerror("'%s' already defined in %s(%d)",
				tzSym, nsym->tzFileName, nsym->nFileLine);
		}
	} else {
		nsym = createsymbol(tzSym);
	}

	if (nsym) {
		nsym->pMacro = malloc(strlen(tzValue) + 1);

		if (nsym->pMacro != NULL)
			strcpy(nsym->pMacro, tzValue);
		else
			fatalerror("No memory for stringequate");

		nsym->nType |= SYMF_STRING | SYMF_DEFINED;
		nsym->ulMacroSize = strlen(tzValue);
		nsym->pScope = NULL;
	}
}

/*
 * check if symbol is a string equated symbol
 */
uint32_t sym_isString(char *tzSym)
{
	const struct sSymbol *pSym = findsymbol(tzSym, NULL);

	if (pSym != NULL) {
		if (pSym->nType & SYMF_STRING)
			return 1;
	}
	return 0;
}

/*
 * Alter a SET symbols value
 */
void sym_AddSet(char *tzSym, int32_t value)
{
	struct sSymbol *nsym = findsymbol(tzSym, NULL);

	if (nsym == NULL) {
		/* Symbol hasn been found, create */
		nsym = createsymbol(tzSym);
	}

	if (nsym) {
		nsym->nValue = value;
		nsym->nType |= SYMF_SET | SYMF_DEFINED | SYMF_CONST;
		nsym->pScope = NULL;
	}
}

/*
 * Add a local (.name) relocatable symbol
 */
void sym_AddLocalReloc(char *tzSym)
{
	if (pScope) {
		if (strlen(tzSym) + strlen(pScope->tzName) > MAXSYMLEN)
			fatalerror("Symbol too long");

		char fullname[MAXSYMLEN + 1];

		fullSymbolName(fullname, sizeof(fullname), tzSym, pScope);
		sym_AddReloc(fullname);

	} else {
		fatalerror("Local label in main scope");
	}
}

/*
 * Add a relocatable symbol
 */
void sym_AddReloc(char *tzSym)
{
	struct sSymbol *scope = NULL;

	if ((nPass == 1)
	    || ((nPass == 2) && (sym_isDefined(tzSym) == 0))) {
		/* only add reloc symbols in pass 1 */
		struct sSymbol *nsym;
		char *localPtr = strchr(tzSym, '.');

		if (localPtr != NULL) {
			if (!pScope)
				fatalerror("Local label in main scope");

			struct sSymbol *parent = pScope->pScope ?
						 pScope->pScope : pScope;
			uint32_t parentLen = localPtr - tzSym;

			if (strchr(localPtr + 1, '.') != NULL) {
				fatalerror("'%s' is a nonsensical reference to a nested local symbol",
					   tzSym);
			} else if (strlen(parent->tzName) != parentLen
				   || strncmp(tzSym, parent->tzName, parentLen) != 0) {
				yyerror("Not currently in the scope of '%.*s'",
					parentLen, tzSym);
			}

			scope = parent;
		}

		nsym = findsymbol(tzSym, scope);

		if (nsym != NULL) {
			if (nsym->nType & SYMF_DEFINED) {
				yyerror("'%s' already defined in %s(%d)", tzSym,
					nsym->tzFileName, nsym->nFileLine);
			}
		} else {
			nsym = createsymbol(tzSym);
		}

		if (nsym) {
			nsym->nValue = nPC;
			nsym->nType |= SYMF_RELOC | SYMF_DEFINED;
			if (localPtr)
				nsym->nType |= SYMF_LOCAL;

			if (exportall)
				nsym->nType |= SYMF_EXPORT;

			nsym->pScope = scope;
			nsym->pSection = pCurrentSection;
		}
	}
	pScope = findsymbol(tzSym, scope);
}

/*
 * Check if the subtraction of two symbols is defined. That is, either both
 * symbols are defined and the result is a constant, or both symbols are
 * relocatable and belong to the same section.
 *
 * It returns 1 if the difference is defined, 0 if not.
 */
int32_t sym_IsRelocDiffDefined(char *tzSym1, char *tzSym2)
{
	/* Do nothing the first pass. */
	if (nPass != 2)
		return 1;

	const struct sSymbol *nsym1 = sym_FindSymbol(tzSym1);
	const struct sSymbol *nsym2 = sym_FindSymbol(tzSym2);

	/* Do the symbols exist? */
	if (nsym1 == NULL)
		fatalerror("Symbol \"%s\" isn't defined.", tzSym1);

	if (nsym2 == NULL)
		fatalerror("Symbol \"%s\" isn't defined.", tzSym2);

	int32_t s1reloc = (nsym1->nType & SYMF_RELOC) != 0;
	int32_t s2reloc = (nsym2->nType & SYMF_RELOC) != 0;

	/* Both are non-relocatable */
	if (!s1reloc && !s2reloc)
		return 1;

	/* One of them is relocatable, the other one is not. */
	if (s1reloc ^ s2reloc)
		return 0;

	/*
	 * Both of them are relocatable. Make sure they are defined (internal
	 * coherency with sym_AddReloc and sym_AddLocalReloc).
	 */
	if (!(nsym1->nType & SYMF_DEFINED))
		fatalerror("Relocatable symbol \"%s\" isn't defined.", tzSym1);

	if (!(nsym2->nType & SYMF_DEFINED))
		fatalerror("Relocatable symbol \"%s\" isn't defined.", tzSym2);

	/*
	 * Both of them must be in the same section for the difference to be
	 * defined.
	 */
	return nsym1->pSection == nsym2->pSection;
}

/*
 * Export a symbol
 */
void sym_Export(char *tzSym)
{
	if (nPass == 1) {
		/* only export symbols in pass 1 */
		struct sSymbol *nsym = sym_FindSymbol(tzSym);

		if (nsym == NULL)
			nsym = createsymbol(tzSym);

		if (nsym)
			nsym->nType |= SYMF_EXPORT;
	} else {
		const struct sSymbol *nsym = sym_FindSymbol(tzSym);

		if (nsym != NULL) {
			if (nsym->nType & SYMF_DEFINED)
				return;
		}
		yyerror("'%s' not defined", tzSym);
	}
}

/*
 * Globalize a symbol (export if defined, import if not)
 */
void sym_Global(char *tzSym)
{
	if (nPass == 2) {
		/* only globalize symbols in pass 2 */
		struct sSymbol *nsym = sym_FindSymbol(tzSym);

		if ((nsym == NULL) || ((nsym->nType & SYMF_DEFINED) == 0)) {
			if (nsym == NULL)
				nsym = createsymbol(tzSym);

			if (nsym)
				nsym->nType |= SYMF_IMPORT;
		} else {
			if (nsym)
				nsym->nType |= SYMF_EXPORT;
		}
	}
}

/*
 * Add a macro definition
 */
void sym_AddMacro(char *tzSym)
{
	if ((nPass == 1) || ((nPass == 2) && (sym_isDefined(tzSym) == 0))) {
		/* only add macros in pass 1 */
		struct sSymbol *nsym;

		nsym = findsymbol(tzSym, NULL);

		if (nsym != NULL) {
			if (nsym->nType & SYMF_DEFINED) {
				yyerror("'%s' already defined in %s(%d)",
					tzSym, nsym->tzFileName,
					nsym->nFileLine);
			}
		} else {
			nsym = createsymbol(tzSym);
		}

		if (nsym) {
			nsym->nValue = nPC;
			nsym->nType |= SYMF_MACRO | SYMF_DEFINED;
			nsym->pScope = NULL;
			nsym->ulMacroSize = ulNewMacroSize;
			nsym->pMacro = tzNewMacro;
		}
	}
}

/*
 * Set whether to export all relocable symbols by default
 */
void sym_SetExportAll(uint8_t set)
{
	exportall = set;
}

/*
 * Prepare for pass #1
 */
void sym_PrepPass1(void)
{
	sym_Init();
}

/*
 * Prepare for pass #2
 */
void sym_PrepPass2(void)
{
	int32_t i;

	for (i = 0; i < HASHSIZE; i += 1) {
		struct sSymbol **ppSym = &(tHashedSymbols[i]);

		while (*ppSym) {
			uint32_t mask = SYMF_SET | SYMF_STRING | SYMF_EQU;

			if ((*ppSym)->nType & mask) {
				struct sSymbol *pTemp;

				pTemp = (*ppSym)->pNext;
				free(*ppSym);
				*ppSym = pTemp;
			} else {
				ppSym = &((*ppSym)->pNext);
			}
		}
	}
	pScope = NULL;
	pPCSymbol->nValue = 0;

	sym_AddString("__TIME__", SavedTIME);
	sym_AddString("__DATE__", SavedDATE);
	sym_AddString("__ISO_8601_LOCAL__", SavedTIMESTAMP_ISO8601_LOCAL);
	sym_AddString("__ISO_8601_UTC__", SavedTIMESTAMP_ISO8601_UTC);
	sym_AddString("__UTC_DAY__", SavedDAY);
	sym_AddString("__UTC_MONTH__", SavedMONTH);
	sym_AddString("__UTC_YEAR__", SavedYEAR);
	sym_AddString("__UTC_HOUR__", SavedHOUR);
	sym_AddString("__UTC_MINUTE__", SavedMINUTE);
	sym_AddString("__UTC_SECOND__", SavedSECOND);
	sym_AddEqu("__RGBDS_MAJOR__", PACKAGE_VERSION_MAJOR);
	sym_AddEqu("__RGBDS_MINOR__", PACKAGE_VERSION_MINOR);
	sym_AddEqu("__RGBDS_PATCH__", PACKAGE_VERSION_PATCH);
	sym_AddSet("_RS", 0);

	sym_AddEqu("_NARG", 0);
	p_NARGSymbol = findsymbol("_NARG", NULL);
	p_NARGSymbol->Callback = Callback_NARG;
	sym_AddEqu("__LINE__", 0);
	p__LINE__Symbol = findsymbol("__LINE__", NULL);
	p__LINE__Symbol->Callback = Callback__LINE__;

	math_DefinePI();
}

/*
 * Initialize the symboltable
 */
void sym_Init(void)
{
	int32_t i;
	time_t now;

	for (i = 0; i < MAXMACROARGS; i += 1) {
		currentmacroargs[i] = NULL;
		newmacroargs[i] = NULL;
	}

	for (i = 0; i < HASHSIZE; i += 1)
		tHashedSymbols[i] = NULL;

	sym_AddReloc("@");
	pPCSymbol = findsymbol("@", NULL);
	sym_AddEqu("_NARG", 0);
	p_NARGSymbol = findsymbol("_NARG", NULL);
	p_NARGSymbol->Callback = Callback_NARG;
	sym_AddEqu("__LINE__", 0);
	p__LINE__Symbol = findsymbol("__LINE__", NULL);
	p__LINE__Symbol->Callback = Callback__LINE__;

	sym_AddSet("_RS", 0);

	if (time(&now) != -1) {
		const struct tm *time_local = localtime(&now);

		strftime(SavedTIME, sizeof(SavedTIME), "\"%H:%M:%S\"",
			 time_local);
		strftime(SavedDATE, sizeof(SavedDATE), "\"%d %B %Y\"",
			 time_local);
		strftime(SavedTIMESTAMP_ISO8601_LOCAL,
			 sizeof(SavedTIMESTAMP_ISO8601_LOCAL), "\"%FT%T%z\"",
			 time_local);

		const struct tm *time_utc = gmtime(&now);

		strftime(SavedTIMESTAMP_ISO8601_UTC,
			 sizeof(SavedTIMESTAMP_ISO8601_UTC), "\"%FT%TZ\"",
			 time_utc);

		strftime(SavedDAY, sizeof(SavedDAY), "%d", time_utc);
		strftime(SavedMONTH, sizeof(SavedMONTH), "%m", time_utc);
		strftime(SavedYEAR, sizeof(SavedYEAR), "%Y", time_utc);
		strftime(SavedHOUR, sizeof(SavedHOUR), "%H", time_utc);
		strftime(SavedMINUTE, sizeof(SavedMINUTE), "%M", time_utc);
		strftime(SavedSECOND, sizeof(SavedSECOND), "%S", time_utc);

		helper_RemoveLeadingZeros(SavedDAY);
		helper_RemoveLeadingZeros(SavedMONTH);
		helper_RemoveLeadingZeros(SavedHOUR);
		helper_RemoveLeadingZeros(SavedMINUTE);
		helper_RemoveLeadingZeros(SavedSECOND);
	} else {
		warnx("Couldn't determine current time.");
		/*
		 * The '?' have to be escaped or they will be treated as
		 * trigraphs...
		 */
		snprintf(SavedTIME, sizeof(SavedTIME),
			 "\"\?\?:\?\?:\?\?\"");
		snprintf(SavedDATE, sizeof(SavedDATE),
			 "\"\?\? \?\?\? \?\?\?\?\"");
		snprintf(SavedTIMESTAMP_ISO8601_LOCAL,
			 sizeof(SavedTIMESTAMP_ISO8601_LOCAL),
			 "\"\?\?\?\?-\?\?-\?\?T\?\?:\?\?:\?\?+\?\?\?\?\"");
		snprintf(SavedTIMESTAMP_ISO8601_UTC,
			 sizeof(SavedTIMESTAMP_ISO8601_UTC),
			 "\"\?\?\?\?-\?\?-\?\?T\?\?:\?\?:\?\?Z\"");
		snprintf(SavedDAY, sizeof(SavedDAY), "1");
		snprintf(SavedMONTH, sizeof(SavedMONTH), "1");
		snprintf(SavedYEAR, sizeof(SavedYEAR), "1900");
		snprintf(SavedHOUR, sizeof(SavedHOUR), "0");
		snprintf(SavedMINUTE, sizeof(SavedMINUTE), "0");
		snprintf(SavedSECOND, sizeof(SavedSECOND), "0");
	}

	sym_AddString("__TIME__", SavedTIME);
	sym_AddString("__DATE__", SavedDATE);
	sym_AddString("__ISO_8601_LOCAL__", SavedTIMESTAMP_ISO8601_LOCAL);
	sym_AddString("__ISO_8601_UTC__", SavedTIMESTAMP_ISO8601_UTC);
	sym_AddString("__UTC_DAY__", SavedDAY);
	sym_AddString("__UTC_MONTH__", SavedMONTH);
	sym_AddString("__UTC_YEAR__", SavedYEAR);
	sym_AddString("__UTC_HOUR__", SavedHOUR);
	sym_AddString("__UTC_MINUTE__", SavedMINUTE);
	sym_AddString("__UTC_SECOND__", SavedSECOND);

	pScope = NULL;

	math_DefinePI();
}
