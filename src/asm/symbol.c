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

static char const *labelScope; /* Current section's label scope */
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
	return lexer_GetLineNo();
}

static int32_t CallbackPC(void)
{
	struct Section const *section = sect_GetSymbolSection();

	return section ? section->org + sect_GetSymbolOffset() : 0;
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
		return sym->value + sym_GetSection(sym)->org;

	return sym->value;
}

/*
 * Update a symbol's definition filename and line
 */
static void updateSymbolFilename(struct Symbol *sym)
{
	if (snprintf(sym->fileName, _MAX_PATH + 1, "%s",
		     lexer_GetFileName()) > _MAX_PATH)
		fatalerror("%s: File name is too long: '%s'\n", __func__,
			   lexer_GetFileName());
	sym->fileLine = fstk_GetLine();
}

/*
 * Create a new symbol by name
 */
static struct Symbol *createsymbol(char const *s)
{
	struct Symbol *symbol = malloc(sizeof(*symbol));

	if (!symbol)
		fatalerror("Failed to create symbol '%s': %s\n", s, strerror(errno));

	if (snprintf(symbol->name, MAXSYMLEN + 1, "%s", s) > MAXSYMLEN)
		warning(WARNING_LONG_STR, "Symbol name is too long: '%s'\n", s);

	symbol->isExported = false;
	symbol->isBuiltin = false;
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
			   char const *localName, char const *scopeName)
{
	int ret = snprintf(output, outputSize, "%s%s", scopeName, localName);

	if (ret < 0)
		fatalerror("snprintf error when expanding symbol name: %s", strerror(errno));
	else if ((size_t)ret >= outputSize)
		fatalerror("Symbol name is too long: '%s%s'\n", scopeName, localName);
}

/*
 * Find a symbol by name and scope
 */
static struct Symbol *findsymbol(char const *s, char const *scope)
{
	char fullname[MAXSYMLEN + 1];

	if (s[0] == '.' && scope) {
		fullSymbolName(fullname, sizeof(fullname), s, scope);
		s = fullname;
	}

	char const *separator = strchr(s, '.');

	if (separator && strchr(separator + 1, '.'))
		fatalerror("'%s' is a nonsensical reference to a nested local symbol\n", s);

	return hash_GetElement(symbols, s);
}

/*
 * Find a symbol by name, with automatically determined scope
 */
struct Symbol *sym_FindSymbol(char const *symName)
{
	return findsymbol(symName, symName[0] == '.' ? labelScope : NULL);
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
	struct Symbol *symbol = sym_FindSymbol(symName);

	if (!symbol) {
		error("'%s' not defined\n", symName);
	} else if (symbol->isBuiltin) {
		error("Built-in symbol '%s' cannot be purged\n", symName);
	} else if (isReferenced(symbol)) {
		error("Symbol \"%s\" is referenced and thus cannot be purged\n", symName);
	} else {
		/* Do not keep a reference to the label's name after purging it */
		if (symbol->name == labelScope)
			labelScope = NULL;

		hash_RemoveElement(symbols, symbol->name);
		free(symbol);
	}
}

uint32_t sym_GetPCValue(void)
{
	struct Section const *sect = sect_GetSymbolSection();

	if (!sect)
		error("PC has no value outside a section\n");
	else if (sect->org == -1)
		error("Expected constant PC but section is not fixed\n");
	else
		return CallbackPC();
	return 0;
}

/*
 * Return a constant symbol's value, assuming it's defined
 */
uint32_t sym_GetConstantSymValue(struct Symbol const *sym)
{
	if (sym == PCSymbol)
		return sym_GetPCValue();
	else if (!sym_IsConstant(sym))
		error("\"%s\" does not have a constant value\n", sym->name);
	else
		return sym_GetValue(sym);

	return 0;
}

/*
 * Return a constant symbol's value
 */
uint32_t sym_GetConstantValue(char const *s)
{
	struct Symbol const *sym = sym_FindSymbol(s);

	if (sym == NULL)
		error("'%s' not defined\n", s);
	else
		return sym_GetConstantSymValue(sym);

	return 0;
}

/*
 * Return a defined symbols value... aborts if not defined yet
 */
uint32_t sym_GetDefinedValue(char const *s)
{
	struct Symbol const *sym = sym_FindSymbol(s);

	if (sym == NULL || !sym_IsDefined(sym))
		error("'%s' not defined\n", s);
	else if (!sym_IsNumeric(sym))
		error("'%s' is a macro or string symbol\n", s);
	else
		return sym_GetValue(sym);

	return 0;
}

char const *sym_GetCurrentSymbolScope(void)
{
	return labelScope;
}

void sym_SetCurrentSymbolScope(char const *newScope)
{
	labelScope = newScope;
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
		error("'%s' already defined at %s(%" PRIu32 ")\n", symbolName,
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
		fatalerror("No memory for string equate: %s\n", strerror(errno));
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
		error("'%s' already defined as %s at %s(%" PRIu32 ")\n",
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
 * Add a label (aka "relocatable symbol")
 * @param name The label's full name (so `.name` is invalid)
 * @return The created symbol
 */
static struct Symbol *addSectionlessLabel(char const *name)
{
	assert(name[0] != '.'); /* The symbol name must have been expanded prior */
	struct Symbol *sym = findsymbol(name, NULL); /* Due to this, don't look for expansions */

	if (!sym) {
		sym = createsymbol(name);
	} else if (sym_IsDefined(sym)) {
		error("'%s' already defined in %s(%" PRIu32 ")\n",
		      name, sym->fileName, sym->fileLine);
		return NULL;
	}
	/* If the symbol already exists as a ref, just "take over" it */
	sym->type = SYM_LABEL;
	sym->callback = NULL;
	sym->value = sect_GetSymbolOffset();
	if (exportall)
		sym->isExported = true;
	sym->section = sect_GetSymbolSection();
	updateSymbolFilename(sym);

	return sym;
}

static struct Symbol *addLabel(char const *name)
{
	struct Symbol *sym = addSectionlessLabel(name);

	if (sym && !sym->section)
		error("Label \"%s\" created outside of a SECTION\n", name);
	return sym;
}

/*
 * Add a local (.name or Parent.name) relocatable symbol
 */
struct Symbol *sym_AddLocalLabel(char const *name)
{
	if (!labelScope) {
		error("Local label '%s' in main scope\n", name);
		return NULL;
	}

	char fullname[MAXSYMLEN + 1];

	if (name[0] == '.') {
		/* If symbol is of the form `.name`, expand to the full `Parent.name` name */
		fullSymbolName(fullname, sizeof(fullname), name, labelScope);
		name = fullname; /* Use the expanded name instead */
	} else {
		size_t i = 0;

		/* Otherwise, check that `Parent` is in fact the current scope */
		while (labelScope[i] && name[i] == labelScope[i])
			i++;
		/* Assuming no dots in `labelScope` */
		assert(strchr(&name[i], '.')); /* There should be at least one dot, though */
		size_t parentLen = i + (strchr(&name[i], '.') - name);

		/*
		 * Check that `labelScope[i]` ended the check, guaranteeing that `name` is at least
		 * as long, and then that this was the entirety of the `Parent` part of `name`.
		 */
		if (labelScope[i] != '\0' || name[i] != '.')
			error("Not currently in the scope of '%.*s'\n", parentLen, name);
		if (strchr(&name[parentLen + 1], '.')) /* There will at least be a terminator */
			fatalerror("'%s' is a nonsensical reference to a nested local label\n",
				   name);
	}

	return addLabel(name);
}

/*
 * Add a relocatable symbol
 */
struct Symbol *sym_AddLabel(char const *name)
{
	struct Symbol *sym = addLabel(name);

	/* Set the symbol as the new scope */
	if (sym)
		labelScope = sym->name;
	return sym;
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
struct Symbol *sym_AddMacro(char const *symName, int32_t defLineNo, char *body, size_t size)
{
	struct Symbol *sym = createNonrelocSymbol(symName);

	sym->type = SYM_MACRO;
	sym->macroSize = size;
	sym->macro = body;
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

		if (symName[0] == '.') {
			if (!labelScope)
				fatalerror("Local label reference '%s' in main scope\n", symName);
			fullSymbolName(fullname, sizeof(fullname), symName, labelScope);
			symName = fullname;
		}

		nsym = createsymbol(symName);
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
	struct Symbol *_NARGSymbol = sym_AddEqu("_NARG", 0);
	struct Symbol *__LINE__Symbol = sym_AddEqu("__LINE__", 0);

	PCSymbol = addSectionlessLabel("@");
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

	labelScope = NULL;

	math_DefinePI();
}
