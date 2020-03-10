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
#include "asm/section.h"
#include "asm/util.h"
#include "asm/warning.h"

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

static int32_t Callback_NARG(struct sSymbol const *self)
{
	(void)self;
	uint32_t i = 0;

	while (currentmacroargs[i] && i < MAXMACROARGS)
		i++;

	return i;
}

static int32_t Callback__LINE__(struct sSymbol const *self)
{
	(void)self;
	return nLineNo;
}

static int32_t CallbackPC(struct sSymbol const *self)
{
	return self->pSection ? self->pSection->nOrg + self->pSection->nPC : 0;
}

/*
 * Get the nValue field of a symbol
 */
int32_t sym_GetValue(struct sSymbol const *sym)
{
	if (sym->Callback)
		return sym->Callback(sym);

	if (sym->type == SYM_LABEL)
		return sym->nValue + sym->pSection->nOrg;

	return sym->nValue;
}

/*
 * Calculate the hash value for a symbol name
 */
uint32_t sym_CalcHash(const char *s)
{
	return calchash(s) % HASHSIZE;
}

/*
 * Update a symbol's definition filename and line
 */
static void updateSymbolFilename(struct sSymbol *nsym)
{
	if (snprintf(nsym->tzFileName, _MAX_PATH + 1, "%s",
		     tzCurrentFileName) > _MAX_PATH) {
		fatalerror("%s: File name is too long: '%s'", __func__,
			   tzCurrentFileName);
	}
	nsym->nFileLine = fstk_GetLine();
}

/*
 * Create a new symbol by name
 */
static struct sSymbol *createsymbol(char const *s)
{
	struct sSymbol **ppsym;
	uint32_t hash;

	hash = sym_CalcHash(s);
	ppsym = &(tHashedSymbols[hash]);

	while ((*ppsym) != NULL)
		ppsym = &((*ppsym)->pNext);

	(*ppsym) = malloc(sizeof(struct sSymbol));

	if ((*ppsym) == NULL) {
		fatalerror("No memory for symbol");
		return NULL;
	}

	if (snprintf((*ppsym)->tzName, MAXSYMLEN + 1, "%s", s) > MAXSYMLEN)
		warning(WARNING_LONG_STR, "Symbol name is too long: '%s'", s);

	(*ppsym)->isConstant = false;
	(*ppsym)->isExported = false;
	(*ppsym)->isBuiltin = false;
	(*ppsym)->pScope = NULL;
	(*ppsym)->pNext = NULL;
	(*ppsym)->pSection = NULL;
	(*ppsym)->nValue = 0;
	(*ppsym)->pMacro = NULL;
	(*ppsym)->Callback = NULL;
	updateSymbolFilename(*ppsym);
	return *ppsym;
}

/*
 * Creates the full name of a local symbol in a given scope, by prepending
 * the name with the parent symbol's name.
 */
static void fullSymbolName(char *output, size_t outputSize,
			   char const *localName, const struct sSymbol *scope)
{
	const struct sSymbol *parent = scope->pScope ? scope->pScope : scope;
	int n = snprintf(output, outputSize, "%s%s", parent->tzName, localName);

	if (n >= (int)outputSize)
		fatalerror("Symbol name is too long: '%s%s'",
			   parent->tzName, localName);
}

/*
 * Find the pointer to a symbol by name and scope
 */
static struct sSymbol **findpsymbol(char const *s, struct sSymbol const *scope)
{
	struct sSymbol **ppsym;
	int32_t hash;
	char fullname[MAXSYMLEN + 1];

	if (s[0] == '.' && scope) {
		fullSymbolName(fullname, sizeof(fullname), s, scope);
		s = fullname;
	}

	char const *separator = strchr(s, '.');

	if (separator) {
		if (strchr(separator + 1, '.'))
			fatalerror("'%s' is a nonsensical reference to a nested local symbol",
				   s);
	}

	hash = sym_CalcHash(s);
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
static struct sSymbol *findsymbol(char const *s, struct sSymbol const *scope)
{
	struct sSymbol **ppsym = findpsymbol(s, scope);

	return ppsym ? *ppsym : NULL;
}

/*
 * Find a symbol by name, with automatically determined scope
 */
struct sSymbol *sym_FindSymbol(char const *tzName)
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
void sym_Purge(char const *tzName)
{
	struct sSymbol **ppSym;
	struct sSymbol *pscope;

	if (*tzName == '.')
		pscope = pScope;
	else
		pscope = NULL;

	ppSym = findpsymbol(tzName, pscope);

	if (!ppSym) {
		yyerror("'%s' not defined", tzName);
	} else if ((*ppSym)->isBuiltin) {
		yyerror("Built-in symbol '%s' cannot be purged", tzName);
	} else {
		struct sSymbol *pSym;

		pSym = *ppSym;
		*ppSym = pSym->pNext;

		if (pSym->pMacro)
			free(pSym->pMacro);

		free(pSym);
	}
}

/*
 * Get a string equate's value
 */
char *sym_GetStringValue(struct sSymbol const *sym)
{
	if (sym != NULL)
		return sym->pMacro;

	yyerror("String symbol '%s' not defined", sym->tzName);

	return NULL;
}

/*
 * Return a constant symbols value
 */
uint32_t sym_GetConstantValue(char const *s)
{
	struct sSymbol const *psym = sym_FindSymbol(s);

	if (psym == pPCSymbol) {
		if (pCurrentSection->nOrg == -1)
			yyerror("Expected constant PC but section is not fixed");
		else
			return sym_GetValue(psym);

	} else if (psym != NULL) {
		if (sym_IsConstant(psym))
			return sym_GetValue(psym);

		yyerror("\"%s\" does not have a constant value", s);
	} else {
		yyerror("'%s' not defined", s);
	}

	return 0;
}

/*
 * Return a defined symbols value... aborts if not defined yet
 */
uint32_t sym_GetDefinedValue(char const *s)
{
	struct sSymbol const *psym = sym_FindSymbol(s);

	if (psym != NULL) {
		if (sym_IsDefined(psym)) {
			if (!sym_IsNumeric(psym))
				yyerror("'%s' is a macro or string symbol", s);

			return sym_GetValue(psym);
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
	for (i = 0; i < MAXMACROARGS - 1; i++)
		currentmacroargs[i] = currentmacroargs[i + 1];

	currentmacroargs[MAXMACROARGS - 1] = NULL;
}

char *sym_FindMacroArg(int32_t i)
{
	if (i == -1)
		i = MAXMACROARGS + 1;

	assert(i >= 1);

	assert((size_t)(i - 1)
	       < sizeof(currentmacroargs) / sizeof(*currentmacroargs));

	return currentmacroargs[i - 1];
}

void sym_UseNewMacroArgs(void)
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		free(currentmacroargs[i]);
		currentmacroargs[i] = newmacroargs[i];
		newmacroargs[i] = NULL;
	}
}

void sym_SaveCurrentMacroArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		save[i] = currentmacroargs[i];
		currentmacroargs[i] = NULL;
	}
}

void sym_RestoreCurrentMacroArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		free(currentmacroargs[i]);
		currentmacroargs[i] = save[i];
	}
}

void sym_AddNewMacroArg(char const *s)
{
	int32_t i = 0;

	while (i < MAXMACROARGS && newmacroargs[i] != NULL)
		i++;

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

	snprintf(s, sizeof(s) - 1, "_%u", nMacroCount);
	newmacroargs[MAXMACROARGS] = strdup(s);
}

void sym_UseCurrentMacroArgs(void)
{
	int32_t i;

	for (i = 1; i <= MAXMACROARGS; i++)
		sym_AddNewMacroArg(sym_FindMacroArg(i));
}

/*
 * Find a macro by name
 */
struct sSymbol *sym_FindMacro(char const *s)
{
	return findsymbol(s, NULL);
}

/*
 * Create a symbol that will be non-relocatable and ensure that it
 * hasn't already been defined or referenced in a context that would
 * require that it be relocatable
 */
static struct sSymbol *createNonrelocSymbol(char const *tzSym)
{
	struct sSymbol *nsym = findsymbol(tzSym, NULL);

	if (nsym != NULL) {
		if (sym_IsDefined(nsym)) {
			yyerror("'%s' already defined at %s(%u)",
				tzSym, nsym->tzFileName, nsym->nFileLine);
		} else {
			yyerror("'%s' referenced as label at %s(%u)",
				tzSym, nsym->tzFileName, nsym->nFileLine);
		}
	} else {
		nsym = createsymbol(tzSym);
	}

	return nsym;
}

/*
 * Add an equated symbol
 */
struct sSymbol *sym_AddEqu(char const *tzSym, int32_t value)
{
	struct sSymbol *nsym = createNonrelocSymbol(tzSym);

	nsym->nValue = value;
	nsym->type = SYM_EQU;
	nsym->isConstant = true;
	nsym->pScope = NULL;
	updateSymbolFilename(nsym);

	return nsym;
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
struct sSymbol *sym_AddString(char const *tzSym, char const *tzValue)
{
	struct sSymbol *nsym = createNonrelocSymbol(tzSym);

	nsym->pMacro = malloc(strlen(tzValue) + 1);

	if (nsym->pMacro != NULL)
		strcpy(nsym->pMacro, tzValue);
	else
		fatalerror("No memory for string equate");

	nsym->type = SYM_EQUS;
	nsym->ulMacroSize = strlen(tzValue);
	nsym->pScope = NULL;

	return nsym;
}

/*
 * Alter a SET symbols value
 */
struct sSymbol *sym_AddSet(char const *tzSym, int32_t value)
{
	struct sSymbol *nsym = findsymbol(tzSym, NULL);

	if (nsym != NULL) {
		if (sym_IsDefined(nsym)) {
			if (nsym->type == SYM_LABEL)
				yyerror("'%s' already defined as non-constant at %s(%u)",
					tzSym,
					nsym->tzFileName,
					nsym->nFileLine);
			else if (nsym->type != SYM_SET)
				yyerror("'%s' already defined as constant at %s(%u)",
					tzSym,
					nsym->tzFileName,
					nsym->nFileLine);
		} else if (nsym->type == SYM_REF) {
			yyerror("'%s' already referenced at %s(%u)",
				tzSym,
				nsym->tzFileName,
				nsym->nFileLine);
		}
	} else {
		nsym = createsymbol(tzSym);
	}

	nsym->nValue = value;
	nsym->type = SYM_SET;
	nsym->isConstant = true;
	nsym->pScope = NULL;
	updateSymbolFilename(nsym);

	return nsym;
}

/*
 * Add a local (.name) relocatable symbol
 */
struct sSymbol *sym_AddLocalReloc(char const *tzSym)
{
	if (pScope) {
		char fullname[MAXSYMLEN + 1];

		fullSymbolName(fullname, sizeof(fullname), tzSym, pScope);
		return sym_AddReloc(fullname);
	} else {
		yyerror("Local label '%s' in main scope", tzSym);
		return NULL;
	}
}

/*
 * Add a relocatable symbol
 */
struct sSymbol *sym_AddReloc(char const *tzSym)
{
	struct sSymbol *scope = NULL;
	struct sSymbol *nsym;
	char *localPtr = strchr(tzSym, '.');

	if (localPtr != NULL) {
		if (!pScope) {
			yyerror("Local label in main scope");
			return NULL;
		}

		struct sSymbol *parent = pScope->pScope ?
					 pScope->pScope : pScope;
		uint32_t parentLen = localPtr - tzSym;

		if (strchr(localPtr + 1, '.') != NULL)
			fatalerror("'%s' is a nonsensical reference to a nested local symbol",
				   tzSym);
		else if (strlen(parent->tzName) != parentLen
			|| strncmp(tzSym, parent->tzName, parentLen) != 0)
			yyerror("Not currently in the scope of '%.*s'",
				parentLen, tzSym);

		scope = parent;
	}

	nsym = findsymbol(tzSym, scope);

	if (!nsym)
		nsym = createsymbol(tzSym);
	else if (sym_IsDefined(nsym))
		yyerror("'%s' already defined in %s(%d)", tzSym,
			nsym->tzFileName, nsym->nFileLine);

	nsym->nValue = nPC;
	nsym->type = SYM_LABEL;
	nsym->isConstant = pCurrentSection && pCurrentSection->nOrg != -1;

	if (exportall)
		nsym->isExported = true;

	nsym->pScope = scope;
	nsym->pSection = sect_GetSymbolSection();
	/* Labels need to be assigned a section, except PC */
	if (!pCurrentSection && strcmp(tzSym, "@"))
		yyerror("Label \"%s\" created outside of a SECTION",
			tzSym);

	updateSymbolFilename(nsym);

	pScope = findsymbol(tzSym, scope);
	return pScope;
}

/*
 * Check if the subtraction of two symbols is defined. That is, either both
 * symbols are defined and the result is a constant, or both symbols are
 * relocatable and belong to the same section.
 *
 * It returns 1 if the difference is defined, 0 if not.
 */
bool sym_IsRelocDiffDefined(char const *tzSym1, char const *tzSym2)
{
	const struct sSymbol *nsym1 = sym_FindSymbol(tzSym1);
	const struct sSymbol *nsym2 = sym_FindSymbol(tzSym2);

	/* Do the symbols exist? */
	if (nsym1 == NULL)
		fatalerror("Symbol \"%s\" isn't defined.", tzSym1);

	if (nsym2 == NULL)
		fatalerror("Symbol \"%s\" isn't defined.", tzSym2);

	int32_t s1const = sym_IsConstant(nsym1);
	int32_t s2const = sym_IsConstant(nsym2);

	/* Both are non-relocatable */
	if (s1const && s2const)
		return true;

	/* One of them is relocatable, the other one is not. */
	if (s1const ^ s2const)
		return false;

	/*
	 * Both of them are relocatable. Make sure they are defined (internal
	 * coherency with sym_AddReloc and sym_AddLocalReloc).
	 */
	if (!sym_IsDefined(nsym1))
		fatalerror("Label \"%s\" isn't defined.", tzSym1);

	if (!sym_IsDefined(nsym2))
		fatalerror("Label \"%s\" isn't defined.", tzSym2);

	/*
	 * Both of them must be in the same section for the difference to be
	 * defined.
	 */
	return nsym1->pSection == nsym2->pSection;
}

/*
 * Export a symbol
 */
void sym_Export(char const *tzSym)
{
	sym_Ref(tzSym);
	struct sSymbol *nsym = sym_FindSymbol(tzSym);

	nsym->isExported = true;
}

/*
 * Add a macro definition
 */
struct sSymbol *sym_AddMacro(char const *tzSym, int32_t nDefLineNo)
{
	struct sSymbol *nsym = createNonrelocSymbol(tzSym);

	nsym->type = SYM_MACRO;
	nsym->pScope = NULL;
	nsym->ulMacroSize = ulNewMacroSize;
	nsym->pMacro = tzNewMacro;
	updateSymbolFilename(nsym);
	/*
	 * The symbol is created at the line after the `endm`,
	 * override this with the actual definition line
	 */
	nsym->nFileLine = nDefLineNo;

	return nsym;
}

/*
 * Flag that a symbol is referenced in an RPN expression
 * and create it if it doesn't exist yet
 */
void sym_Ref(char const *tzSym)
{
	struct sSymbol *nsym = sym_FindSymbol(tzSym);

	if (nsym == NULL) {
		char fullname[MAXSYMLEN + 1];

		if (*tzSym == '.') {
			if (!pScope)
				fatalerror("Local label reference '%s' in main scope",
					   tzSym);
			fullSymbolName(fullname, sizeof(fullname), tzSym,
				       pScope);
			tzSym = fullname;
		}

		nsym = createsymbol(tzSym);
		nsym->type = SYM_REF;
	}
}

/*
 * Set whether to export all relocatable symbols by default
 */
void sym_SetExportAll(bool set)
{
	exportall = set;
}

/**
 * Returns a pointer to the first non-zero character in a string
 * Non-'0', not non-'\0'.
 */
static inline char const *removeLeadingZeros(char const *ptr)
{
	while (*ptr == '0')
		ptr++;
	return ptr;
}

/*
 * Initialize the symboltable
 */
void sym_Init(void)
{
	int32_t i;

	for (i = 0; i < MAXMACROARGS; i++) {
		currentmacroargs[i] = NULL;
		newmacroargs[i] = NULL;
	}

	for (i = 0; i < HASHSIZE; i++)
		tHashedSymbols[i] = NULL;

	pPCSymbol = sym_AddReloc("@");
	pPCSymbol->Callback = CallbackPC;
	pPCSymbol->isBuiltin = true;
	p_NARGSymbol = sym_AddEqu("_NARG", 0);
	p_NARGSymbol->Callback = Callback_NARG;
	p_NARGSymbol->isBuiltin = true;
	p__LINE__Symbol = sym_AddEqu("__LINE__", 0);
	p__LINE__Symbol->Callback = Callback__LINE__;
	p__LINE__Symbol->isBuiltin = true;
	struct sSymbol *_RSSymbol = sym_AddSet("_RS", 0);

	_RSSymbol->isBuiltin = true;

	time_t now = time(NULL);

	if (now == (time_t)-1) {
		warn("Couldn't determine current time");
		/* Fall back by pretending we are at the Epoch */
		now = 0;
	}

	const struct tm *time_utc = gmtime(&now);
	const struct tm *time_local = localtime(&now);

	strftime(SavedTIME, sizeof(SavedTIME), "\"%H:%M:%S\"",
		 time_local);
	strftime(SavedDATE, sizeof(SavedDATE), "\"%d %B %Y\"",
		 time_local);
	strftime(SavedTIMESTAMP_ISO8601_LOCAL,
		 sizeof(SavedTIMESTAMP_ISO8601_LOCAL), "\"%Y-%m-%dT%H-%M-%S%z\"",
		 time_local);

	strftime(SavedTIMESTAMP_ISO8601_UTC,
		 sizeof(SavedTIMESTAMP_ISO8601_UTC), "\"%Y-%m-%dT%H-%M-%SZ\"",
		 time_utc);

	strftime(SavedYEAR, sizeof(SavedYEAR), "%Y", time_utc);
	strftime(SavedMONTH, sizeof(SavedMONTH), "%m", time_utc);
	strftime(SavedDAY, sizeof(SavedDAY), "%d", time_utc);
	strftime(SavedHOUR, sizeof(SavedHOUR), "%H", time_utc);
	strftime(SavedMINUTE, sizeof(SavedMINUTE), "%M", time_utc);
	strftime(SavedSECOND, sizeof(SavedSECOND), "%S", time_utc);

#define addString(name, val) do { \
	struct sSymbol *symbol = sym_AddString(name, val); \
	symbol->isBuiltin = true; \
} while (0)
	addString("__TIME__", SavedTIME);
	addString("__DATE__", SavedDATE);
	addString("__ISO_8601_LOCAL__", SavedTIMESTAMP_ISO8601_LOCAL);
	addString("__ISO_8601_UTC__", SavedTIMESTAMP_ISO8601_UTC);
	/* This cannot start with zeros */
	addString("__UTC_YEAR__", SavedYEAR);
	addString("__UTC_MONTH__", removeLeadingZeros(SavedMONTH));
	addString("__UTC_DAY__", removeLeadingZeros(SavedDAY));
	addString("__UTC_HOUR__", removeLeadingZeros(SavedHOUR));
	addString("__UTC_MINUTE__", removeLeadingZeros(SavedMINUTE));
	addString("__UTC_SECOND__", removeLeadingZeros(SavedSECOND));
#undef addString

	pScope = NULL;

	math_DefinePI();
}
