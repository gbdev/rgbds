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
#include <inttypes.h>
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

static struct Symbol *symbolScope; /* Current section symbol scope */
static struct Symbol *PCSymbol;
static char savedTIME[256];
static char savedDATE[256];
static char savedTIMESTAMP_ISO8601_LOCAL[256];
static char savedTIMESTAMP_ISO8601_UTC[256];
static char savedDAY[3];
static char savedMONTH[3];
static char savedYEAR[20];
static char savedHOUR[3];
static char savedMINUTE[3];
static char savedSECOND[3];
static bool exportall;

bool sym_IsPC(struct Symbol const *sym)
{
	return sym == PCSymbol;
}

struct ForEachArgs {
	void (*func)(struct Symbol *symbol, void *arg);
	void *arg;
};

static void forEachWrapper(void *_symbol, void *_argWrapper)
{
	struct ForEachArgs *argWrapper = _argWrapper;
	struct Symbol *symbol = _symbol;

	argWrapper->func(symbol, argWrapper->arg);
}

void sym_ForEach(void (*func)(struct Symbol *, void *), void *arg)
{
	struct ForEachArgs argWrapper = { .func = func, .arg = arg };

	hash_ForEach(symbols, forEachWrapper, &argWrapper);
}

static int32_t Callback_NARG(void)
{
	return macro_NbArgs();
}

static int32_t Callback__LINE__(void)
{
	return nLineNo;
}

static int32_t CallbackPC(void)
{
	struct Section const *section = sect_GetSymbolSection();

	return section ? section->nOrg + curOffset : 0;
}

/*
 * Get the value field of a symbol
 */
int32_t sym_GetValue(struct Symbol const *sym)
{
	if (sym_IsNumeric(sym) && sym->callback)
		return sym->callback();

	if (sym->type == SYM_LABEL)
		/* TODO: do not use section's org directly */
		return sym->value + sym_GetSection(sym)->nOrg;

	return sym->value;
}

/*
 * Update a symbol's definition filename and line
 */
static void updateSymbolFilename(struct Symbol *sym)
{
	if (snprintf(sym->fileName, _MAX_PATH + 1, "%s",
		     tzCurrentFileName) > _MAX_PATH)
		fatalerror("%s: File name is too long: '%s'", __func__,
			   tzCurrentFileName);
	sym->fileLine = fstk_GetLine();
}

/*
 * Create a new symbol by name
 */
static struct Symbol *createsymbol(char const *s)
{
	struct Symbol *symbol = malloc(sizeof(*symbol));

	if (!symbol)
		fatalerror("Failed to create symbol: %s", strerror(errno));

	if (snprintf(symbol->name, MAXSYMLEN + 1, "%s", s) > MAXSYMLEN)
		warning(WARNING_LONG_STR, "Symbol name is too long: '%s'", s);

	symbol->isExported = false;
	symbol->isBuiltin = false;
	symbol->scope = NULL;
	symbol->section = NULL;
	updateSymbolFilename(symbol);
	symbol->ID = -1;
	symbol->next = NULL;

	hash_AddElement(symbols, symbol->name, symbol);
	return symbol;
}

/*
 * Creates the full name of a local symbol in a given scope, by prepending
 * the name with the parent symbol's name.
 */
static void fullSymbolName(char *output, size_t outputSize,
			   char const *localName, const struct Symbol *scope)
{
	const struct Symbol *parent = scope->scope ? scope->scope : scope;
	int n = snprintf(output, outputSize, "%s%s", parent->name, localName);

	if (n >= (int)outputSize)
		fatalerror("Symbol name is too long: '%s%s'", parent->name,
			   localName);
}

/*
 * Find a symbol by name and scope
 */
static struct Symbol *findsymbol(char const *s, struct Symbol const *scope)
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
struct Symbol *sym_FindSymbol(char const *symName)
{
	return findsymbol(symName, symName[0] == '.' ? symbolScope : NULL);
}

static inline bool isReferenced(struct Symbol const *sym)
{
	return sym->ID != -1;
}

/*
 * Purge a symbol
 */
void sym_Purge(char const *symName)
{
	struct Symbol *scope = symName[0] == '.' ? symbolScope : NULL;
	struct Symbol *symbol = findsymbol(symName, scope);

	if (!symbol) {
		yyerror("'%s' not defined", symName);
	} else if (symbol->isBuiltin) {
		yyerror("Built-in symbol '%s' cannot be purged", symName);
	} else if (isReferenced(symbol)) {
		yyerror("Symbol \"%s\" is referenced and thus cannot be purged",
			symName);
	} else {
		hash_RemoveElement(symbols, symbol->name);
		if (symbol->type == SYM_MACRO)
			free(symbol->macro);
		free(symbol);
	}
}

uint32_t sym_GetPCValue(void)
{
	struct Section const *sect = sect_GetSymbolSection();

	if (!sect)
		yyerror("PC has no value outside a section");
	else if (sect->nOrg == -1)
		yyerror("Expected constant PC but section is not fixed");
	else
		return CallbackPC();
	return 0;
}

/*
 * Return a constant symbols value
 */
uint32_t sym_GetConstantValue(char const *s)
{
	struct Symbol const *sym = sym_FindSymbol(s);

	if (sym == NULL)
		yyerror("'%s' not defined", s);
	else if (sym == PCSymbol)
		return sym_GetPCValue();
	else if (!sym_IsConstant(sym))
		yyerror("\"%s\" does not have a constant value", s);
	else
		return sym_GetValue(sym);

	return 0;
}

/*
 * Return a defined symbols value... aborts if not defined yet
 */
uint32_t sym_GetDefinedValue(char const *s)
{
	struct Symbol const *sym = sym_FindSymbol(s);

	if (sym == NULL || !sym_IsDefined(sym))
		yyerror("'%s' not defined", s);
	else if (!sym_IsNumeric(sym))
		yyerror("'%s' is a macro or string symbol", s);
	else
		return sym_GetValue(sym);

	return 0;
}

struct Symbol *sym_GetCurrentSymbolScope(void)
{
	return symbolScope;
}

void sym_SetCurrentSymbolScope(struct Symbol *newScope)
{
	symbolScope = newScope;
}

/*
 * Create a symbol that will be non-relocatable and ensure that it
 * hasn't already been defined or referenced in a context that would
 * require that it be relocatable
 */
static struct Symbol *createNonrelocSymbol(char const *symbolName)
{
	struct Symbol *symbol = findsymbol(symbolName, NULL);

	if (!symbol)
		symbol = createsymbol(symbolName);
	else if (sym_IsDefined(symbol))
		yyerror("'%s' already defined at %s(%" PRIu32 ")", symbolName,
			symbol->fileName, symbol->fileLine);

	return symbol;
}

/*
 * Add an equated symbol
 */
struct Symbol *sym_AddEqu(char const *symName, int32_t value)
{
	struct Symbol *sym = createNonrelocSymbol(symName);

	sym->type = SYM_EQU;
	sym->callback = NULL;
	sym->value = value;

	return sym;
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
struct Symbol *sym_AddString(char const *symName, char const *value)
{
	struct Symbol *sym = createNonrelocSymbol(symName);
	size_t len = strlen(value);
	char *string = malloc(len + 1);

	if (string == NULL)
		fatalerror("No memory for string equate");
	strcpy(string, value);

	sym->type = SYM_EQUS;
	/* TODO: use other fields */
	sym->macroSize = len;
	sym->macro = string;

	return sym;
}

/*
 * Alter a SET symbols value
 */
struct Symbol *sym_AddSet(char const *symName, int32_t value)
{
	struct Symbol *sym = findsymbol(symName, NULL);

	if (sym == NULL)
		sym = createsymbol(symName);
	else if (sym_IsDefined(sym) && sym->type != SYM_SET)
		yyerror("'%s' already defined as %s at %s(%" PRIu32 ")",
			symName, sym->type == SYM_LABEL ? "label" : "constant",
			sym->fileName, sym->fileLine);
	else
		/* TODO: can the scope be incorrect when talking over refs? */
		updateSymbolFilename(sym);

	sym->type = SYM_SET;
	sym->callback = NULL;
	sym->value = value;

	return sym;
}

/*
 * Add a local (.name) relocatable symbol
 */
struct Symbol *sym_AddLocalReloc(char const *symName)
{
	if (!symbolScope) {
		yyerror("Local label '%s' in main scope", symName);
		return NULL;
	}

	char fullname[MAXSYMLEN + 1];

	fullSymbolName(fullname, sizeof(fullname), symName, symbolScope);
	return sym_AddReloc(fullname);
}

/*
 * Add a relocatable symbol
 */
struct Symbol *sym_AddReloc(char const *symName)
{
	struct Symbol const *scope = NULL;
	char *localPtr = strchr(symName, '.');

	if (localPtr != NULL) {
		if (!symbolScope) {
			yyerror("Local label in main scope");
			return NULL;
		}

		scope = symbolScope->scope ? symbolScope->scope : symbolScope;
		uint32_t parentLen = localPtr - symName;

		if (strchr(localPtr + 1, '.') != NULL)
			fatalerror("'%s' is a nonsensical reference to a nested local symbol",
				   symName);
		else if (strlen(scope->name) != parentLen
			|| strncmp(symName, scope->name, parentLen) != 0)
			yyerror("Not currently in the scope of '%.*s'",
				parentLen, symName);
	}

	struct Symbol *sym = findsymbol(symName, scope);

	if (!sym)
		sym = createsymbol(symName);
	else if (sym_IsDefined(sym))
		yyerror("'%s' already defined in %s(%" PRIu32 ")", symName,
			sym->fileName, sym->fileLine);
	/* If the symbol already exists as a ref, just "take over" it */

	sym->type = SYM_LABEL;
	sym->callback = NULL;
	sym->value = curOffset;

	if (exportall)
		sym->isExported = true;

	sym->scope = scope;
	sym->section = sect_GetSymbolSection();
	/* Labels need to be assigned a section, except PC */
	if (!sym->section && strcmp(symName, "@"))
		yyerror("Label \"%s\" created outside of a SECTION",
			symName);

	updateSymbolFilename(sym);

	/* Set the symbol as the new scope */
	/* TODO: don't do this for local labels */
	symbolScope = findsymbol(symName, scope);
	return symbolScope;
}

/*
 * Export a symbol
 */
void sym_Export(char const *symName)
{
	struct Symbol *sym = sym_FindSymbol(symName);

	/* If the symbol doesn't exist, create a ref that can be purged */
	if (!sym)
		sym = sym_Ref(symName);
	sym->isExported = true;
}

/*
 * Add a macro definition
 */
struct Symbol *sym_AddMacro(char const *symName, int32_t defLineNo)
{
	struct Symbol *sym = createNonrelocSymbol(symName);

	sym->type = SYM_MACRO;
	sym->macroSize = ulNewMacroSize;
	sym->macro = tzNewMacro;
	updateSymbolFilename(sym);
	/*
	 * The symbol is created at the line after the `endm`,
	 * override this with the actual definition line
	 */
	sym->fileLine = defLineNo;

	return sym;
}

/*
 * Flag that a symbol is referenced in an RPN expression
 * and create it if it doesn't exist yet
 */
struct Symbol *sym_Ref(char const *symName)
{
	struct Symbol *nsym = sym_FindSymbol(symName);

	if (nsym == NULL) {
		char fullname[MAXSYMLEN + 1];
		struct Symbol const *scope = NULL;

		if (symName[0] == '.') {
			if (!symbolScope)
				fatalerror("Local label reference '%s' in main scope",
					   symName);
			scope = symbolScope->scope ? symbolScope->scope
						   : symbolScope;
			fullSymbolName(fullname, sizeof(fullname), symName,
				       symbolScope);
			symName = fullname;
		}

		nsym = createsymbol(symName);
		nsym->type = SYM_REF;
		nsym->scope = scope;
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
	struct Symbol *_NARGSymbol = sym_AddEqu("_NARG", 0);
	struct Symbol *__LINE__Symbol = sym_AddEqu("__LINE__", 0);

	PCSymbol = sym_AddReloc("@"),
	PCSymbol->isBuiltin = true;
	PCSymbol->callback = CallbackPC;
	_NARGSymbol->isBuiltin = true;
	_NARGSymbol->callback = Callback_NARG;
	__LINE__Symbol->isBuiltin = true;
	__LINE__Symbol->callback = Callback__LINE__;
	sym_AddSet("_RS", 0)->isBuiltin = true;

	sym_AddEqu("__RGBDS_MAJOR__", PACKAGE_VERSION_MAJOR)->isBuiltin = true;
	sym_AddEqu("__RGBDS_MINOR__", PACKAGE_VERSION_MINOR)->isBuiltin = true;
	sym_AddEqu("__RGBDS_PATCH__", PACKAGE_VERSION_PATCH)->isBuiltin = true;

	time_t now = time(NULL);

	if (now == (time_t)-1) {
		warn("Couldn't determine current time");
		/* Fall back by pretending we are at the Epoch */
		now = 0;
	}

	const struct tm *time_utc = gmtime(&now);
	const struct tm *time_local = localtime(&now);

	strftime(savedTIME, sizeof(savedTIME), "\"%H:%M:%S\"", time_local);
	strftime(savedDATE, sizeof(savedDATE), "\"%d %B %Y\"", time_local);
	strftime(savedTIMESTAMP_ISO8601_LOCAL,
		 sizeof(savedTIMESTAMP_ISO8601_LOCAL), "\"%Y-%m-%dT%H:%M:%S%z\"",
		 time_local);

	strftime(savedTIMESTAMP_ISO8601_UTC,
		 sizeof(savedTIMESTAMP_ISO8601_UTC), "\"%Y-%m-%dT%H:%M:%SZ\"",
		 time_utc);

	strftime(savedYEAR, sizeof(savedYEAR), "%Y", time_utc);
	strftime(savedMONTH, sizeof(savedMONTH), "%m", time_utc);
	strftime(savedDAY, sizeof(savedDAY), "%d", time_utc);
	strftime(savedHOUR, sizeof(savedHOUR), "%H", time_utc);
	strftime(savedMINUTE, sizeof(savedMINUTE), "%M", time_utc);
	strftime(savedSECOND, sizeof(savedSECOND), "%S", time_utc);

#define addString(name, val) sym_AddString(name, val)->isBuiltin = true
	addString("__TIME__", savedTIME);
	addString("__DATE__", savedDATE);
	addString("__ISO_8601_LOCAL__", savedTIMESTAMP_ISO8601_LOCAL);
	addString("__ISO_8601_UTC__", savedTIMESTAMP_ISO8601_UTC);
	/* This cannot start with zeros */
	addString("__UTC_YEAR__", savedYEAR);
	addString("__UTC_MONTH__", removeLeadingZeros(savedMONTH));
	addString("__UTC_DAY__", removeLeadingZeros(savedDAY));
	addString("__UTC_HOUR__", removeLeadingZeros(savedHOUR));
	addString("__UTC_MINUTE__", removeLeadingZeros(savedMINUTE));
	addString("__UTC_SECOND__", removeLeadingZeros(savedSECOND));
#undef addString

	symbolScope = NULL;

	math_DefinePI();
}
