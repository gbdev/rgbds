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
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "asm/asm.h"
#include "asm/fstack.h"
#include "asm/macro.h"
#include "asm/main.h"
#include "asm/mymath.h"
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/util.h"
#include "asm/warning.h"

#include "extern/err.h"

#include "hashmap.h"
#include "helpers.h"
#include "version.h"

HashMap symbols;

static struct sSymbol *pScope; /* Current section symbol scope */
struct sSymbol *pPCSymbol;
static struct sSymbol *p_NARGSymbol;
static struct sSymbol *p__LINE__Symbol;
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

struct ForEachArgs {
	void (*func)(struct sSymbol *, void *);
	void *arg;
};

static void forEachWrapper(void *_symbol, void *_argWrapper)
{
	struct ForEachArgs *argWrapper = _argWrapper;
	struct sSymbol *symbol = _symbol;

	argWrapper->func(symbol, argWrapper->arg);
}

void sym_ForEach(void (*func)(struct sSymbol *, void *), void *arg)
{
	struct ForEachArgs argWrapper = { .func = func, .arg = arg };

	hash_ForEach(symbols, forEachWrapper, &argWrapper);
}

static int32_t Callback_NARG(struct sSymbol const *self)
{
	(void)self;
	return macro_NbArgs();
}

static int32_t Callback__LINE__(struct sSymbol const *self)
{
	(void)self;
	return nLineNo;
}

static int32_t CallbackPC(struct sSymbol const *self)
{
	return self->pSection ? self->pSection->nOrg + curOffset : 0;
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
	struct sSymbol *symbol = malloc(sizeof(*symbol));

	if (!symbol)
		fatalerror("Failed to create symbol: %s", strerror(errno));

	if (snprintf(symbol->tzName, MAXSYMLEN + 1, "%s", s) > MAXSYMLEN)
		warning(WARNING_LONG_STR, "Symbol name is too long: '%s'", s);

	hash_AddElement(symbols, symbol->tzName, symbol);

	symbol->isExported = false;
	symbol->isBuiltin = false;
	symbol->pScope = NULL;
	symbol->pSection = NULL;
	symbol->nValue = 0; /* TODO: is this necessary? */
	symbol->pMacro = NULL;
	symbol->Callback = NULL;

	symbol->ID = -1;
	symbol->next = NULL;
	updateSymbolFilename(symbol);
	return symbol;
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
 * Find a symbol by name and scope
 */
static struct sSymbol *findsymbol(char const *s, struct sSymbol const *scope)
{
	char fullname[MAXSYMLEN + 1];

	if (s[0] == '.' && scope) {
		fullSymbolName(fullname, sizeof(fullname), s, scope);
		s = fullname;
	}

	char const *separator = strchr(s, '.');

	if (separator && strchr(separator + 1, '.'))
		fatalerror("'%s' is a nonsensical reference to a nested local symbol",
			   s);

	return hash_GetElement(symbols, s);
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

static inline bool isReferenced(struct sSymbol const *sym)
{
	return sym->ID != -1;
}

/*
 * Purge a symbol
 */
void sym_Purge(char const *tzName)
{
	struct sSymbol *scope = tzName[0] == '.' ? pScope : NULL;
	struct sSymbol *symbol = findsymbol(tzName, scope);

	if (!symbol) {
		yyerror("'%s' not defined", tzName);
	} else if (symbol->isBuiltin) {
		yyerror("Built-in symbol '%s' cannot be purged", tzName);
	} else if (isReferenced(symbol)) {
		yyerror("Symbol \"%s\" is referenced and thus cannot be purged",
			tzName);
	} else {
		hash_RemoveElement(symbols, tzName);
		free(symbol->pMacro);
		free(symbol);
	}
}

/*
 * Return a constant symbols value
 */
uint32_t sym_GetConstantValue(char const *s)
{
	struct sSymbol const *psym = sym_FindSymbol(s);

	if (psym == pPCSymbol) {
		if (!pCurrentSection)
			yyerror("PC has no value outside a section");
		else if (pCurrentSection->nOrg == -1)
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
	/* If the symbol already exists as a ref, just "take over" it */

	nsym->nValue = curOffset;
	nsym->type = SYM_LABEL;

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
 * Export a symbol
 */
void sym_Export(char const *tzSym)
{
	struct sSymbol *nsym = sym_FindSymbol(tzSym);

	/* If the symbol doesn't exist, create a ref that can be purged */
	if (!nsym)
		nsym = sym_Ref(tzSym);
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
struct sSymbol *sym_Ref(char const *tzSym)
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

	return nsym;
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

	sym_AddEqu("__RGBDS_MAJOR__", PACKAGE_VERSION_MAJOR);
	sym_AddEqu("__RGBDS_MINOR__", PACKAGE_VERSION_MINOR);
	sym_AddEqu("__RGBDS_PATCH__", PACKAGE_VERSION_PATCH);

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
