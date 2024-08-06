/* SPDX-License-Identifier: MIT */

#include "asm/symbol.hpp"

#include <inttypes.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>

#include "error.hpp"
#include "helpers.hpp" // assume
#include "version.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/macro.hpp"
#include "asm/output.hpp"
#include "asm/warning.hpp"

using namespace std::literals;

std::unordered_map<std::string, Symbol> symbols;
std::unordered_set<std::string> purgedSymbols;

static std::optional<std::string> labelScope = std::nullopt; // Current section's label scope
static Symbol *PCSymbol;
static Symbol *_NARGSymbol;
static Symbol *_RSSymbol;
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
	if (MacroArgs const *macroArgs = fstk_GetCurrentMacroArgs(); macroArgs) {
		return macroArgs->nbArgs();
	} else {
		error("_NARG does not make sense outside of a macro\n");
		return 0;
	}
}

static int32_t CallbackPC() {
	Section const *section = sect_GetSymbolSection();

	return section ? section->org + sect_GetSymbolOffset() : 0;
}

int32_t Symbol::getValue() const {
	assume(std::holds_alternative<int32_t>(data) || std::holds_alternative<int32_t (*)()>(data));
	if (auto *value = std::get_if<int32_t>(&data); value) {
		return type == SYM_LABEL ? *value + getSection()->org : *value;
	}
	return getOutputValue();
}

int32_t Symbol::getOutputValue() const {
	if (auto *value = std::get_if<int32_t>(&data); value) {
		return *value;
	} else if (auto *callback = std::get_if<int32_t (*)()>(&data); callback) {
		return (*callback)();
	} else {
		return 0;
	}
}

ContentSpan const &Symbol::getMacro() const {
	assume((std::holds_alternative<ContentSpan>(data)));
	return std::get<ContentSpan>(data);
}

std::shared_ptr<std::string> Symbol::getEqus() const {
	assume(std::holds_alternative<std::shared_ptr<std::string>>(data));
	return std::get<std::shared_ptr<std::string>>(data);
}

static void dumpFilename(Symbol const &sym) {
	if (sym.src) {
		sym.src->dump(sym.fileLine);
		putc('\n', stderr);
	} else if (sym.isBuiltin) {
		fputs("<builtin>\n", stderr);
	} else {
		fputs("<command-line>\n", stderr);
	}
}

// Update a symbol's definition filename and line
static void updateSymbolFilename(Symbol &sym) {
	std::shared_ptr<FileStackNode> oldSrc = std::move(sym.src);
	sym.src = fstk_GetFileStack();
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0;

	// If the old node was registered, ensure the new one is too
	if (oldSrc && oldSrc->ID != (uint32_t)-1)
		out_RegisterNode(sym.src);
}

// Create a new symbol by name
static Symbol &createSymbol(std::string const &symName) {
	static uint32_t nextDefIndex = 0;

	Symbol &sym = symbols[symName];

	sym.name = symName;
	sym.isExported = false;
	sym.isBuiltin = false;
	sym.section = nullptr;
	sym.src = fstk_GetFileStack();
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0;
	sym.ID = -1;
	sym.defIndex = nextDefIndex++;

	return sym;
}

Symbol *sym_FindExactSymbol(std::string const &symName) {
	auto search = symbols.find(symName);
	return search != symbols.end() ? &search->second : nullptr;
}

Symbol *sym_FindScopedSymbol(std::string const &symName) {
	if (size_t dotPos = symName.find('.'); dotPos != std::string::npos) {
		if (symName.find('.', dotPos + 1) != std::string::npos)
			fatalerror(
			    "'%s' is a nonsensical reference to a nested local symbol\n", symName.c_str()
			);
		// If auto-scoped local label, expand the name
		if (dotPos == 0 && labelScope)
			return sym_FindExactSymbol(*labelScope + symName);
	}
	return sym_FindExactSymbol(symName);
}

Symbol *sym_FindScopedValidSymbol(std::string const &symName) {
	Symbol *sym = sym_FindScopedSymbol(symName);

	// `@` has no value outside a section
	if (sym_IsPC(sym) && !sect_GetSymbolSection()) {
		return nullptr;
	}
	// `_NARG` has no value outside a macro
	if (sym == _NARGSymbol && !fstk_GetCurrentMacroArgs()) {
		return nullptr;
	}
	return sym;
}

Symbol const *sym_GetPC() {
	return PCSymbol;
}

void sym_Purge(std::string const &symName) {
	Symbol *sym = sym_FindScopedValidSymbol(symName);

	if (!sym) {
		if (sym_IsPurgedScoped(symName))
			error("'%s' was already purged\n", symName.c_str());
		else
			error("'%s' not defined\n", symName.c_str());
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be purged\n", symName.c_str());
	} else if (sym->ID != (uint32_t)-1) {
		error("Symbol \"%s\" is referenced and thus cannot be purged\n", symName.c_str());
	} else {
		if (sym->isExported)
			warning(WARNING_PURGE_1, "Purging an exported symbol \"%s\"\n", symName.c_str());
		else if (sym->isLabel())
			warning(WARNING_PURGE_2, "Purging a label \"%s\"\n", symName.c_str());
		// Do not keep a reference to the label's name after purging it
		if (sym->name == labelScope)
			labelScope = std::nullopt;
		purgedSymbols.emplace(sym->name);
		symbols.erase(sym->name);
	}
}

bool sym_IsPurgedExact(std::string const &symName) {
	return purgedSymbols.find(symName) != purgedSymbols.end();
}

bool sym_IsPurgedScoped(std::string const &symName) {
	if (size_t dotPos = symName.find('.'); dotPos != std::string::npos) {
		// Check for a nonsensical reference to a nested scoped symbol
		if (symName.find('.', dotPos + 1) != std::string::npos)
			return false;
		// If auto-scoped local label, expand the name
		if (dotPos == 0 && labelScope)
			return sym_IsPurgedExact(*labelScope + symName);
	}
	return sym_IsPurgedExact(symName);
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

int32_t sym_GetRSValue() {
	return _RSSymbol->getOutputValue();
}

void sym_SetRSValue(int32_t value) {
	updateSymbolFilename(*_RSSymbol);
	_RSSymbol->data = value;
}

// Return a constant symbol's value, assuming it's defined
uint32_t Symbol::getConstantValue() const {
	if (sym_IsPC(this))
		return sym_GetPCValue();

	if (isConstant())
		return getValue();

	error("\"%s\" does not have a constant value\n", name.c_str());
	return 0;
}

// Return a constant symbol's value
uint32_t sym_GetConstantValue(std::string const &symName) {
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym)
		return sym->getConstantValue();

	if (sym_IsPurgedScoped(symName))
		error("'%s' not defined; it was purged\n", symName.c_str());
	else
		error("'%s' not defined\n", symName.c_str());
	return 0;
}

std::optional<std::string> const &sym_GetCurrentSymbolScope() {
	return labelScope;
}

void sym_SetCurrentSymbolScope(std::optional<std::string> const &newScope) {
	labelScope = newScope;
}

/*
 * Create a symbol that will be non-relocatable and ensure that it
 * hasn't already been defined or referenced in a context that would
 * require that it be relocatable
 * @param symName The name of the symbol to create
 * @param numeric If false, the symbol may not have been referenced earlier
 */
static Symbol *createNonrelocSymbol(std::string const &symName, bool numeric) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
		purgedSymbols.erase(sym->name);
	} else if (sym->isDefined()) {
		error("'%s' already defined at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	} else if (!numeric) {
		// The symbol has already been referenced, but it's not allowed
		error("'%s' already referenced at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	}

	return sym;
}

// Add an equated symbol
Symbol *sym_AddEqu(std::string const &symName, int32_t value) {
	Symbol *sym = createNonrelocSymbol(symName, true);

	if (!sym)
		return nullptr;

	sym->type = SYM_EQU;
	sym->data = value;

	return sym;
}

Symbol *sym_RedefEqu(std::string const &symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym)
		return sym_AddEqu(symName, value);

	if (sym->isDefined() && sym->type != SYM_EQU) {
		error("'%s' already defined as non-EQU at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr;
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be redefined\n", symName.c_str());
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
 * quotes inside the string, like sym_AddString("name"s, "\"test\"), or the
 * assembler won't be able to use it with DB and similar. This is equivalent to
 * ``` name EQUS "\"test\"" ```
 *
 * If the desired symbol is a register or a number, just the terminator quotes
 * of the string are enough: sym_AddString("M_PI"s, "3.1415"). This is the same
 * as ``` M_PI EQUS "3.1415" ```
 */
Symbol *sym_AddString(std::string const &symName, std::shared_ptr<std::string> str) {
	Symbol *sym = createNonrelocSymbol(symName, false);

	if (!sym)
		return nullptr;

	sym->type = SYM_EQUS;
	sym->data = str;
	return sym;
}

Symbol *sym_RedefString(std::string const &symName, std::shared_ptr<std::string> str) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym)
		return sym_AddString(symName, str);

	if (sym->type != SYM_EQUS) {
		if (sym->isDefined())
			error("'%s' already defined as non-EQUS at ", symName.c_str());
		else
			error("'%s' already referenced at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr;
	} else if (sym->isBuiltin) {
		error("Built-in symbol '%s' cannot be redefined\n", symName.c_str());
		return nullptr;
	}

	updateSymbolFilename(*sym);
	sym->data = str;

	return sym;
}

// Alter a mutable symbol's value
Symbol *sym_AddVar(std::string const &symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
	} else if (sym->isDefined() && sym->type != SYM_VAR) {
		error(
		    "'%s' already defined as %s at ",
		    symName.c_str(),
		    sym->type == SYM_LABEL ? "label" : "constant"
		);
		dumpFilename(*sym);
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
static Symbol *addLabel(std::string const &symName) {
	assume(!symName.starts_with('.')); // The symbol name must have been expanded prior
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
	} else if (sym->isDefined()) {
		error("'%s' already defined at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr;
	} else {
		updateSymbolFilename(*sym);
	}
	// If the symbol already exists as a ref, just "take over" it
	sym->type = SYM_LABEL;
	sym->data = (int32_t)sect_GetSymbolOffset();
	// Don't export anonymous labels
	if (exportAll && !symName.starts_with('!'))
		sym->isExported = true;
	sym->section = sect_GetSymbolSection();

	if (sym && !sym->section)
		error("Label \"%s\" created outside of a SECTION\n", symName.c_str());

	return sym;
}

// Add a local (`.name` or `Parent.name`) relocatable symbol
Symbol *sym_AddLocalLabel(std::string const &symName) {
	// Assuming no dots in `labelScope` if defined
	assume(!labelScope.has_value() || labelScope->find('.') == std::string::npos);

	size_t dotPos = symName.find('.');

	assume(dotPos != std::string::npos); // There should be at least one dot in `symName`

	// Check for something after the dot
	if (dotPos == symName.length() - 1) {
		fatalerror("'%s' is a nonsensical reference to an empty local label\n", symName.c_str());
	}
	// Check for more than one dot
	if (symName.find('.', dotPos + 1) != std::string::npos)
		fatalerror("'%s' is a nonsensical reference to a nested local label\n", symName.c_str());

	if (dotPos == 0) {
		if (!labelScope.has_value()) {
			error("Unqualified local label '%s' in main scope\n", symName.c_str());
			return nullptr;
		}
		return addLabel(*labelScope + symName);
	}
	return addLabel(symName);
}

// Add a relocatable symbol
Symbol *sym_AddLabel(std::string const &symName) {
	Symbol *sym = addLabel(symName);

	// Set the symbol as the new scope
	if (sym)
		labelScope = sym->name;
	return sym;
}

static uint32_t anonLabelID = 0;

// Add an anonymous label
Symbol *sym_AddAnonLabel() {
	if (anonLabelID == UINT32_MAX) {
		error("Only %" PRIu32 " anonymous labels can be created!", anonLabelID);
		return nullptr;
	}

	std::string anon = sym_MakeAnonLabelName(0, true); // The direction is important!
	anonLabelID++;
	return addLabel(anon);
}

// Write an anonymous label's name to a buffer
std::string sym_MakeAnonLabelName(uint32_t ofs, bool neg) {
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

	std::string anon("!");
	anon += std::to_string(id);
	return anon;
}

// Export a symbol
void sym_Export(std::string const &symName) {
	if (symName.starts_with('!')) {
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
Symbol *sym_AddMacro(std::string const &symName, int32_t defLineNo, ContentSpan const &span) {
	Symbol *sym = createNonrelocSymbol(symName, false);

	if (!sym)
		return nullptr;

	sym->type = SYM_MACRO;
	sym->data = span;

	sym->src = fstk_GetFileStack();
	// The symbol is created at the line after the `ENDM`,
	// override this with the actual definition line
	sym->fileLine = defLineNo;

	return sym;
}

// Flag that a symbol is referenced in an RPN expression
// and create it if it doesn't exist yet
Symbol *sym_Ref(std::string const &symName) {
	Symbol *sym = sym_FindScopedSymbol(symName);

	if (!sym) {
		if (symName.starts_with('.')) {
			if (!labelScope.has_value())
				fatalerror("Local label reference '%s' in main scope\n", symName.c_str());
			std::string fullName = *labelScope + symName;

			sym = &createSymbol(fullName);
		} else {
			sym = &createSymbol(symName);
		}

		sym->type = SYM_REF;
	}

	return sym;
}

// Set whether to export all relocatable symbols by default
void sym_SetExportAll(bool set) {
	exportAll = set;
}

// Define the built-in symbols
void sym_Init(time_t now) {
	PCSymbol = &createSymbol("@"s);
	PCSymbol->type = SYM_LABEL;
	PCSymbol->data = CallbackPC;
	PCSymbol->isBuiltin = true;

	_NARGSymbol = &createSymbol("_NARG"s);
	_NARGSymbol->type = SYM_EQU;
	_NARGSymbol->data = Callback_NARG;
	_NARGSymbol->isBuiltin = true;

	_RSSymbol = sym_AddVar("_RS"s, 0);
	_RSSymbol->isBuiltin = true;

	sym_AddString("__RGBDS_VERSION__"s, std::make_shared<std::string>(get_package_version_string()))
	    ->isBuiltin = true;
	sym_AddEqu("__RGBDS_MAJOR__"s, PACKAGE_VERSION_MAJOR)->isBuiltin = true;
	sym_AddEqu("__RGBDS_MINOR__"s, PACKAGE_VERSION_MINOR)->isBuiltin = true;
	sym_AddEqu("__RGBDS_PATCH__"s, PACKAGE_VERSION_PATCH)->isBuiltin = true;
#ifdef PACKAGE_VERSION_RC
	sym_AddEqu("__RGBDS_RC__"s, PACKAGE_VERSION_RC)->isBuiltin = true;
#endif

	if (now == (time_t)-1) {
		warn("Failed to determine current time");
		// Fall back by pretending we are at the Epoch
		now = 0;
	}

	tm const *time_local = localtime(&now);

	strftime(savedTIME, sizeof(savedTIME), "\"%H:%M:%S\"", time_local);
	strftime(savedDATE, sizeof(savedDATE), "\"%d %B %Y\"", time_local);
	strftime(
	    savedTIMESTAMP_ISO8601_LOCAL,
	    sizeof(savedTIMESTAMP_ISO8601_LOCAL),
	    "\"%Y-%m-%dT%H:%M:%S%z\"",
	    time_local
	);

	tm const *time_utc = gmtime(&now);

	strftime(
	    savedTIMESTAMP_ISO8601_UTC,
	    sizeof(savedTIMESTAMP_ISO8601_UTC),
	    "\"%Y-%m-%dT%H:%M:%SZ\"",
	    time_utc
	);

	sym_AddString("__TIME__"s, std::make_shared<std::string>(savedTIME))->isBuiltin = true;
	sym_AddString("__DATE__"s, std::make_shared<std::string>(savedDATE))->isBuiltin = true;
	sym_AddString(
	    "__ISO_8601_LOCAL__"s, std::make_shared<std::string>(savedTIMESTAMP_ISO8601_LOCAL)
	)
	    ->isBuiltin = true;
	sym_AddString("__ISO_8601_UTC__"s, std::make_shared<std::string>(savedTIMESTAMP_ISO8601_UTC))
	    ->isBuiltin = true;

	sym_AddEqu("__UTC_YEAR__"s, time_utc->tm_year + 1900)->isBuiltin = true;
	sym_AddEqu("__UTC_MONTH__"s, time_utc->tm_mon + 1)->isBuiltin = true;
	sym_AddEqu("__UTC_DAY__"s, time_utc->tm_mday)->isBuiltin = true;
	sym_AddEqu("__UTC_HOUR__"s, time_utc->tm_hour)->isBuiltin = true;
	sym_AddEqu("__UTC_MINUTE__"s, time_utc->tm_min)->isBuiltin = true;
	sym_AddEqu("__UTC_SECOND__"s, time_utc->tm_sec)->isBuiltin = true;
}
