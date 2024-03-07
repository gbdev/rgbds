/* SPDX-License-Identifier: MIT */

#include "asm/symbol.hpp"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <map>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <string_view>
#include <time.h>
#include <variant>

#include "error.hpp"
#include "helpers.hpp"
#include "util.hpp"
#include "version.hpp"

#include "asm/fixpoint.hpp"
#include "asm/fstack.hpp"
#include "asm/macro.hpp"
#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/warning.hpp"

std::map<std::string, Symbol> symbols;

static const char *labelScope; // Current section's label scope
static Symbol *PCSymbol;
static Symbol *_NARGSymbol;
static char savedTIME[256];
static char savedDATE[256];
static char savedTIMESTAMP_ISO8601_LOCAL[256];
static char savedTIMESTAMP_ISO8601_UTC[256];
static bool exportAll;

bool sym_IsPC(Symbol const *sym) {
	return sym == PCSymbol;
}

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols)
		callback(it.second);
}

static int32_t Callback_NARG() {
	if (!macro_GetCurrentArgs()) {
		error("_NARG does not make sense outside of a macro\n");
		return 0;
	}
	return macro_NbArgs();
}

static int32_t CallbackPC() {
	Section const *section = sect_GetSymbolSection();

	return section ? section->org + sect_GetSymbolOffset() : 0;
}

int32_t Symbol::getValue() const {
	assert(std::holds_alternative<int32_t>(data) || std::holds_alternative<int32_t (*)()>(data));
	if (int32_t const *value = std::get_if<int32_t>(&data); value) {
		// TODO: do not use section's org directly
		return type == SYM_LABEL ? *value + getSection()->org : *value;
	}
	return getOutputValue();
}

int32_t Symbol::getOutputValue() const {
	return std::visit(
	    Visitor{
	        [](int32_t value) -> int32_t { return value; },
	        [](int32_t (*callback)()) -> int32_t { return callback(); },
	        [](auto &) -> int32_t { return 0; },
	    },
	    data
	);
}

std::string_view *Symbol::getMacro() const {
	assert(std::holds_alternative<std::string_view *>(data));
	return std::get<std::string_view *>(data);
}

std::string *Symbol::getEqus() const {
	assert(std::holds_alternative<std::string *>(data));
	return std::get<std::string *>(data);
}

static void dumpFilename(Symbol const &sym) {
	if (sym.src)
		sym.src->dump(sym.fileLine);
	else if (sym.fileLine == 0)
		fputs("<command-line>", stderr);
	else
		fputs("<builtin>", stderr);
}

// Set a symbol's definition filename and line
static void setSymbolFilename(Symbol &sym) {
	sym.src = fstk_GetFileStack();                  // This is `nullptr` for built-ins
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0; // This is 1 for built-ins
}

// Update a symbol's definition filename and line
static void updateSymbolFilename(Symbol &sym) {
	FileStackNode *oldSrc = sym.src;

	setSymbolFilename(sym);
	// If the old node was referenced, ensure the new one is
	if (oldSrc && oldSrc->referenced && oldSrc->ID != (uint32_t)-1)
		out_RegisterNode(sym.src);
	// TODO: unref the old node, and use `out_ReplaceNode` instead of deleting it
}

// Create a new symbol by name
static Symbol &createsymbol(char const *symName) {
	Symbol &sym = symbols[symName];

	if (snprintf(sym.name, MAXSYMLEN + 1, "%s", symName) > MAXSYMLEN)
		warning(WARNING_LONG_STR, "Symbol name is too long: '%s'\n", symName);

	sym.isExported = false;
	sym.isBuiltin = false;
	sym.section = nullptr;
	setSymbolFilename(sym);
	sym.ID = -1;

	return sym;
}

// Creates the full name of a local symbol in a given scope, by prepending
// the name with the parent symbol's name.
static void
    fullSymbolName(char *output, size_t outputSize, char const *localName, char const *scopeName) {
	int ret = snprintf(output, outputSize, "%s%s", scopeName, localName);

	if (ret < 0)
		fatalerror("snprintf error when expanding symbol name: %s", strerror(errno));
	else if ((size_t)ret >= outputSize)
		fatalerror("Symbol name is too long: '%s%s'\n", scopeName, localName);
}

static void assignStringSymbol(Symbol &sym, char const *value) {
	std::string *equs = new (std::nothrow) std::string(value);
	if (!equs)
		fatalerror("No memory for string equate: %s\n", strerror(errno));
	sym.type = SYM_EQUS;
	sym.data = equs;
}

Symbol *sym_FindExactSymbol(char const *symName) {
	auto search = symbols.find(symName);
	return search != symbols.end() ? &search->second : nullptr;
}

Symbol *sym_FindScopedSymbol(char const *symName) {
	if (char const *localName = strchr(symName, '.'); localName) {
		if (strchr(localName + 1, '.'))
			fatalerror("'%s' is a nonsensical reference to a nested local symbol\n", symName);
		// If auto-scoped local label, expand the name
		if (localName == symName) { // Meaning, the name begins with the dot
			char fullName[MAXSYMLEN + 1];

			fullSymbolName(fullName, sizeof(fullName), symName, labelScope);
			return sym_FindExactSymbol(fullName);
		}
	}
	return sym_FindExactSymbol(symName);
}

Symbol *sym_FindScopedValidSymbol(char const *symName) {
	Symbol *sym = sym_FindScopedSymbol(symName);

	// `@` has no value outside a section
	if (sym_IsPC(sym) && !sect_GetSymbolSection()) {
		return nullptr;
	}
	// `_NARG` has no value outside a macro
	if (sym == _NARGSymbol && !macro_GetCurrentArgs()) {
		return nullptr;
	}
	return sym;
}

Symbol const *sym_GetPC() {
	return PCSymbol;
}

// Purge a symbol
void sym_Purge(std::string const &symName) {
	Symbol *sym = sym_FindScopedValidSymbol(symName.c_str());

	if (!sym) {
		error("'%s' not defined\n", symName.c_str());
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be purged\n", symName.c_str());
	} else if (sym->ID != (uint32_t)-1) {
		error("Symbol \"%s\" is referenced and thus cannot be purged\n", symName.c_str());
	} else {
		// Do not keep a reference to the label's name after purging it
		if (sym->name == labelScope)
			sym_SetCurrentSymbolScope(nullptr);

		// FIXME: this leaks `sym->getEqus()` for SYM_EQUS and `sym->getMacro()` for SYM_MACRO,
		// but this can't delete either of them because the expansion may be purging itself.
		symbols.erase(sym->name);
		// TODO: ideally, also unref the file stack nodes
	}
}

uint32_t sym_GetPCValue() {
	Section const *sect = sect_GetSymbolSection();

	if (!sect)
		error("PC has no value outside a section\n");
	else if (sect->org == (uint32_t)-1)
		error("Expected constant PC but section is not fixed\n");
	else
		return CallbackPC();
	return 0;
}

// Return a constant symbol's value, assuming it's defined
uint32_t Symbol::getConstantValue() const {
	if (sym_IsPC(this))
		return sym_GetPCValue();

	if (isConstant())
		return getValue();

	error("\"%s\" does not have a constant value\n", name);
	return 0;
}

// Return a constant symbol's value
uint32_t sym_GetConstantValue(char const *symName) {
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym)
		return sym->getConstantValue();

	error("'%s' not defined\n", symName);
	return 0;
}

char const *sym_GetCurrentSymbolScope() {
	return labelScope;
}

void sym_SetCurrentSymbolScope(char const *newScope) {
	labelScope = newScope;
}

/*
 * Create a symbol that will be non-relocatable and ensure that it
 * hasn't already been defined or referenced in a context that would
 * require that it be relocatable
 * @param symName The name of the symbol to create
 * @param numeric If false, the symbol may not have been referenced earlier
 */
static Symbol *createNonrelocSymbol(char const *symName, bool numeric) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createsymbol(symName);
	} else if (sym->isDefined()) {
		error("'%s' already defined at ", symName);
		dumpFilename(*sym);
		putc('\n', stderr);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	} else if (!numeric) {
		// The symbol has already been referenced, but it's not allowed
		error("'%s' already referenced at ", symName);
		dumpFilename(*sym);
		putc('\n', stderr);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	}

	return sym;
}

// Add an equated symbol
Symbol *sym_AddEqu(char const *symName, int32_t value) {
	Symbol *sym = createNonrelocSymbol(symName, true);

	if (!sym)
		return nullptr;

	sym->type = SYM_EQU;
	sym->data = value;

	return sym;
}

Symbol *sym_RedefEqu(char const *symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym)
		return sym_AddEqu(symName, value);

	if (sym->isDefined() && sym->type != SYM_EQU) {
		error("'%s' already defined as non-EQU at ", symName);
		dumpFilename(*sym);
		putc('\n', stderr);
		return nullptr;
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be redefined\n", symName);
		return nullptr;
	}

	updateSymbolFilename(*sym);
	sym->type = SYM_EQU;
	sym->data = value;

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
Symbol *sym_AddString(char const *symName, char const *value) {
	Symbol *sym = createNonrelocSymbol(symName, false);

	if (!sym)
		return nullptr;

	assignStringSymbol(*sym, value);
	return sym;
}

Symbol *sym_RedefString(char const *symName, char const *value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym)
		return sym_AddString(symName, value);

	if (sym->type != SYM_EQUS) {
		if (sym->isDefined())
			error("'%s' already defined as non-EQUS at ", symName);
		else
			error("'%s' already referenced at ", symName);
		dumpFilename(*sym);
		putc('\n', stderr);
		return nullptr;
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be redefined\n", symName);
		return nullptr;
	}

	updateSymbolFilename(*sym);
	// FIXME: this leaks the previous `sym->getEqus()`, but this can't delete it because the
	// expansion may be redefining itself.
	assignStringSymbol(*sym, value);

	return sym;
}

// Alter a mutable symbol's value
Symbol *sym_AddVar(char const *symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createsymbol(symName);
	} else if (sym->isDefined() && sym->type != SYM_VAR) {
		error(
		    "'%s' already defined as %s at ", symName, sym->type == SYM_LABEL ? "label" : "constant"
		);
		dumpFilename(*sym);
		putc('\n', stderr);
		return sym;
	} else {
		updateSymbolFilename(*sym);
	}

	sym->type = SYM_VAR;
	sym->data = value;

	return sym;
}

/*
 * Add a label (aka "relocatable symbol")
 * @param symName The label's full name (so `.name` is invalid)
 * @return The created symbol
 */
static Symbol *addLabel(char const *symName) {
	assert(symName[0] != '.'); // The symbol name must have been expanded prior
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createsymbol(symName);
	} else if (sym->isDefined()) {
		error("'%s' already defined at ", symName);
		dumpFilename(*sym);
		putc('\n', stderr);
		return nullptr;
	} else {
		updateSymbolFilename(*sym);
	}
	// If the symbol already exists as a ref, just "take over" it
	sym->type = SYM_LABEL;
	sym->data = (int32_t)sect_GetSymbolOffset();
	// Don't export anonymous labels
	if (exportAll && symName[0] != '!')
		sym->isExported = true;
	sym->section = sect_GetSymbolSection();

	if (sym && !sym->section)
		error("Label \"%s\" created outside of a SECTION\n", symName);

	return sym;
}

// Add a local (`.name` or `Parent.name`) relocatable symbol
Symbol *sym_AddLocalLabel(char const *symName) {
	// Assuming no dots in `labelScope` if defined
	assert(!labelScope || !strchr(labelScope, '.'));

	char fullName[MAXSYMLEN + 1];
	char const *localName = strchr(symName, '.');

	assert(localName); // There should be at least one dot in `symName`

	// Check for something after the dot in `localName`
	if (localName[1] == '\0') {
		fatalerror("'%s' is a nonsensical reference to an empty local label\n", symName);
	}
	// Check for more than one dot in `localName`
	if (strchr(localName + 1, '.'))
		fatalerror("'%s' is a nonsensical reference to a nested local label\n", symName);

	if (localName == symName) {
		if (!labelScope) {
			error("Unqualified local label '%s' in main scope\n", symName);
			return nullptr;
		}
		// Expand `symName` to the full `labelScope.symName` name
		fullSymbolName(fullName, sizeof(fullName), symName, labelScope);
		symName = fullName;
	}

	return addLabel(symName);
}

// Add a relocatable symbol
Symbol *sym_AddLabel(char const *symName) {
	Symbol *sym = addLabel(symName);

	// Set the symbol as the new scope
	if (sym)
		sym_SetCurrentSymbolScope(sym->name);
	return sym;
}

static uint32_t anonLabelID;

// Add an anonymous label
Symbol *sym_AddAnonLabel() {
	if (anonLabelID == UINT32_MAX) {
		error("Only %" PRIu32 " anonymous labels can be created!", anonLabelID);
		return nullptr;
	}
	char name[MAXSYMLEN + 1];

	sym_WriteAnonLabelName(name, 0, true); // The direction is important!
	anonLabelID++;
	return addLabel(name);
}

// Write an anonymous label's name to a buffer
void sym_WriteAnonLabelName(char buf[MAXSYMLEN + 1], uint32_t ofs, bool neg) {
	uint32_t id = 0;

	if (neg) {
		if (ofs > anonLabelID)
			error(
			    "Reference to anonymous label %" PRIu32 " before, when only %" PRIu32
			    " ha%s been created so far\n",
			    ofs,
			    anonLabelID,
			    anonLabelID == 1 ? "s" : "ve"
			);
		else
			id = anonLabelID - ofs;
	} else {
		ofs--; // We're referencing symbols that haven't been created yet...
		if (ofs > UINT32_MAX - anonLabelID)
			error(
			    "Reference to anonymous label %" PRIu32 " after, when only %" PRIu32
			    " may still be created\n",
			    ofs + 1,
			    UINT32_MAX - anonLabelID
			);
		else
			id = anonLabelID + ofs;
	}

	sprintf(buf, "!%u", id);
}

// Export a symbol
void sym_Export(char const *symName) {
	if (symName[0] == '!') {
		error("Anonymous labels cannot be exported\n");
		return;
	}

	Symbol *sym = sym_FindScopedSymbol(symName);

	// If the symbol doesn't exist, create a ref that can be purged
	if (!sym)
		sym = sym_Ref(symName);
	sym->isExported = true;
}

// Add a macro definition
Symbol *sym_AddMacro(char const *symName, int32_t defLineNo, char const *body, size_t size) {
	Symbol *sym = createNonrelocSymbol(symName, false);

	if (!sym)
		return nullptr;

	std::string_view *macro = new (std::nothrow) std::string_view(body, size);
	if (!macro)
		fatalerror("No memory for macro: %s\n", strerror(errno));
	sym->type = SYM_MACRO;
	sym->data = macro;

	setSymbolFilename(*sym); // TODO: is this really necessary?
	// The symbol is created at the line after the `endm`,
	// override this with the actual definition line
	sym->fileLine = defLineNo;

	return sym;
}

// Flag that a symbol is referenced in an RPN expression
// and create it if it doesn't exist yet
Symbol *sym_Ref(char const *symName) {
	Symbol *sym = sym_FindScopedSymbol(symName);

	if (!sym) {
		char fullname[MAXSYMLEN + 1];

		if (symName[0] == '.') {
			if (!labelScope)
				fatalerror("Local label reference '%s' in main scope\n", symName);
			fullSymbolName(fullname, sizeof(fullname), symName, labelScope);
			symName = fullname;
		}

		sym = &createsymbol(symName);
		sym->type = SYM_REF;
	}

	return sym;
}

// Set whether to export all relocatable symbols by default
void sym_SetExportAll(bool set) {
	exportAll = set;
}

static Symbol *createBuiltinSymbol(char const *symName) {
	Symbol *sym = &createsymbol(symName);

	sym->isBuiltin = true;
	sym->src = nullptr;
	sym->fileLine = 1; // This is 0 for CLI-defined symbols

	return sym;
}

// Initialize the symboltable
void sym_Init(time_t now) {
	PCSymbol = createBuiltinSymbol("@");
	PCSymbol->type = SYM_LABEL;
	PCSymbol->data = CallbackPC;

	_NARGSymbol = createBuiltinSymbol("_NARG");
	_NARGSymbol->type = SYM_EQU;
	_NARGSymbol->data = Callback_NARG;

	sym_AddVar("_RS", 0)->isBuiltin = true;

#define addSym(fn, name, val) \
	do { \
		Symbol *sym = fn(name, val); \
		assert(sym); \
		sym->isBuiltin = true; \
	} while (0)
#define addNumber(name, val) addSym(sym_AddEqu, name, val)
#define addString(name, val) addSym(sym_AddString, name, val)

	addString("__RGBDS_VERSION__", get_package_version_string());
	addNumber("__RGBDS_MAJOR__", PACKAGE_VERSION_MAJOR);
	addNumber("__RGBDS_MINOR__", PACKAGE_VERSION_MINOR);
	addNumber("__RGBDS_PATCH__", PACKAGE_VERSION_PATCH);
#ifdef PACKAGE_VERSION_RC
	addNumber("__RGBDS_RC__", PACKAGE_VERSION_RC);
#endif

	if (now == (time_t)-1) {
		warn("Failed to determine current time");
		// Fall back by pretending we are at the Epoch
		now = 0;
	}

	const tm *time_local = localtime(&now);

	strftime(savedTIME, sizeof(savedTIME), "\"%H:%M:%S\"", time_local);
	strftime(savedDATE, sizeof(savedDATE), "\"%d %B %Y\"", time_local);
	strftime(
	    savedTIMESTAMP_ISO8601_LOCAL,
	    sizeof(savedTIMESTAMP_ISO8601_LOCAL),
	    "\"%Y-%m-%dT%H:%M:%S%z\"",
	    time_local
	);

	const tm *time_utc = gmtime(&now);

	strftime(
	    savedTIMESTAMP_ISO8601_UTC,
	    sizeof(savedTIMESTAMP_ISO8601_UTC),
	    "\"%Y-%m-%dT%H:%M:%SZ\"",
	    time_utc
	);

	addString("__TIME__", savedTIME);
	addString("__DATE__", savedDATE);
	addString("__ISO_8601_LOCAL__", savedTIMESTAMP_ISO8601_LOCAL);
	addString("__ISO_8601_UTC__", savedTIMESTAMP_ISO8601_UTC);

	addNumber("__UTC_YEAR__", time_utc->tm_year + 1900);
	addNumber("__UTC_MONTH__", time_utc->tm_mon + 1);
	addNumber("__UTC_DAY__", time_utc->tm_mday);
	addNumber("__UTC_HOUR__", time_utc->tm_hour);
	addNumber("__UTC_MINUTE__", time_utc->tm_min);
	addNumber("__UTC_SECOND__", time_utc->tm_sec);

#undef addNumber
#undef addString
#undef addSym

	sym_SetCurrentSymbolScope(nullptr);
	anonLabelID = 0;
}
