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

static Symbol const *globalScope = nullptr; // Current section's global label scope
static Symbol const *localScope = nullptr; // Current section's local label scope
static Symbol *PCSymbol;
static Symbol *NARGSymbol;
static Symbol *globalScopeSymbol;
static Symbol *localScopeSymbol;
static Symbol *RSSymbol;
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

static int32_t NARGCallback() {
	if (MacroArgs const *macroArgs = fstk_GetCurrentMacroArgs(); macroArgs) {
		return macroArgs->nbArgs();
	} else {
		error("_NARG has no value outside of a macro\n");
		return 0;
	}
}

static std::shared_ptr<std::string> globalScopeCallback() {
	if (!globalScope) {
		error("\".\" has no value outside of a label scope\n");
		return std::make_shared<std::string>("");
	}
	return std::make_shared<std::string>(globalScope->name);
}

static std::shared_ptr<std::string> localScopeCallback() {
	if (!localScope) {
		error("\"..\" has no value outside of a local label scope\n");
		return std::make_shared<std::string>("");
	}
	return std::make_shared<std::string>(localScope->name);
}

static int32_t PCCallback() {
	return sect_GetSymbolSection()->org + sect_GetSymbolOffset();
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
	assume(
	    std::holds_alternative<std::shared_ptr<std::string>>(data)
	    || std::holds_alternative<std::shared_ptr<std::string> (*)()>(data)
	);
	if (auto *callback = std::get_if<std::shared_ptr<std::string> (*)()>(&data); callback)
		return (*callback)();
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

static void updateSymbolFilename(Symbol &sym) {
	std::shared_ptr<FileStackNode> oldSrc = std::move(sym.src);
	sym.src = fstk_GetFileStack();
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0;

	// If the old node was registered, ensure the new one is too
	if (oldSrc && oldSrc->ID != (uint32_t)-1)
		out_RegisterNode(sym.src);
}

static void alreadyDefinedError(Symbol const &sym, char const *asType) {
	if (sym.isBuiltin && !sym_FindScopedValidSymbol(sym.name)) {
		// `DEF()` would return false, so we should not claim the symbol is already defined
		error("'%s' is reserved for a built-in symbol\n", sym.name.c_str());
	} else {
		error("'%s' already defined", sym.name.c_str());
		if (asType)
			fprintf(stderr, " as %s", asType);
		fputs(" at ", stderr);
		dumpFilename(sym);
	}
}

static void redefinedError(Symbol const &sym) {
	assume(sym.isBuiltin);
	if (!sym_FindScopedValidSymbol(sym.name)) {
		// `DEF()` would return false, so we should not imply the symbol is already defined
		error("'%s' is reserved for a built-in symbol\n", sym.name.c_str());
	} else {
		error("Built-in symbol '%s' cannot be redefined\n", sym.name.c_str());
	}
}

static void assumeAlreadyExpanded(std::string const &symName) {
	// Either the symbol name is `Global.local` or entirely '.'s (for scopes `.` and `..`),
	// but cannot be unqualified `.local`
	assume(!symName.starts_with('.') || symName.find_first_not_of('.') == symName.npos);
}

static Symbol &createSymbol(std::string const &symName) {
	assumeAlreadyExpanded(symName);

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

static bool isAutoScoped(std::string const &symName) {
	// `globalScope` should be global if it's defined
	assume(!globalScope || globalScope->name.find('.') == std::string::npos);
	// `localScope` should be qualified local if it's defined
	assume(!localScope || localScope->name.find('.') != std::string::npos);

	size_t dotPos = symName.find('.');

	// If there are no dots, it's not a local label
	if (dotPos == std::string::npos)
		return false;

	// Label scopes `.` and `..` are the only nonlocal identifiers that start with a dot
	if (dotPos == 0 && symName.find_first_not_of('.') == symName.npos)
		return false;

	// Check for nothing after the dot
	if (dotPos == symName.length() - 1)
		fatalerror("'%s' is a nonsensical reference to an empty local label\n", symName.c_str());

	// Check for more than one dot
	if (symName.find('.', dotPos + 1) != std::string::npos)
		fatalerror("'%s' is a nonsensical reference to a nested local label\n", symName.c_str());

	// Check for already-qualified local label
	if (dotPos > 0)
		return false;

	// Check for unqualifiable local label
	if (!globalScope)
		fatalerror("Unqualified local label '%s' in main scope\n", symName.c_str());

	return true;
}

Symbol *sym_FindExactSymbol(std::string const &symName) {
	assumeAlreadyExpanded(symName);

	auto search = symbols.find(symName);
	return search != symbols.end() ? &search->second : nullptr;
}

Symbol *sym_FindScopedSymbol(std::string const &symName) {
	return sym_FindExactSymbol(isAutoScoped(symName) ? globalScope->name + symName : symName);
}

Symbol *sym_FindScopedValidSymbol(std::string const &symName) {
	Symbol *sym = sym_FindScopedSymbol(symName);

	// `@` has no value outside of a section
	if (sym_IsPC(sym) && !sect_GetSymbolSection()) {
		return nullptr;
	}
	// `_NARG` has no value outside of a macro
	if (sym == NARGSymbol && !fstk_GetCurrentMacroArgs()) {
		return nullptr;
	}
	// `.` has no value outside of a global label scope
	if (sym == globalScopeSymbol && !globalScope) {
		return nullptr;
	}
	// `..` has no value outside of a local label scope
	if (sym == localScopeSymbol && !localScope) {
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
		// Do not keep a reference to the label after purging it
		if (sym == globalScope)
			globalScope = nullptr;
		if (sym == localScope)
			localScope = nullptr;
		purgedSymbols.emplace(sym->name);
		symbols.erase(sym->name);
	}
}

bool sym_IsPurgedExact(std::string const &symName) {
	assumeAlreadyExpanded(symName);

	return purgedSymbols.find(symName) != purgedSymbols.end();
}

bool sym_IsPurgedScoped(std::string const &symName) {
	return sym_IsPurgedExact(isAutoScoped(symName) ? globalScope->name + symName : symName);
}

int32_t sym_GetRSValue() {
	return RSSymbol->getOutputValue();
}

void sym_SetRSValue(int32_t value) {
	updateSymbolFilename(*RSSymbol);
	RSSymbol->data = value;
}

uint32_t Symbol::getConstantValue() const {
	if (isConstant())
		return getValue();

	if (sym_IsPC(this)) {
		if (!getSection())
			error("PC has no value outside of a section\n");
		else
			error("PC does not have a constant value; the current section is not fixed\n");
	} else {
		error("\"%s\" does not have a constant value\n", name.c_str());
	}
	return 0;
}

uint32_t sym_GetConstantValue(std::string const &symName) {
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym)
		return sym->getConstantValue();

	if (sym_IsPurgedScoped(symName))
		error("'%s' not defined; it was purged\n", symName.c_str());
	else
		error("'%s' not defined\n", symName.c_str());
	return 0;
}

std::pair<Symbol const *, Symbol const *> sym_GetCurrentLabelScopes() {
	return {globalScope, localScope};
}

void sym_SetCurrentLabelScopes(std::pair<Symbol const *, Symbol const *> newScopes) {
	globalScope = std::get<0>(newScopes);
	localScope = std::get<1>(newScopes);

	// `globalScope` should be global if it's defined
	assume(!globalScope || globalScope->name.find('.') == std::string::npos);
	// `localScope` should be qualified local if it's defined
	assume(!localScope || localScope->name.find('.') != std::string::npos);
}

void sym_ResetCurrentLabelScopes() {
	globalScope = nullptr;
	localScope = nullptr;
}

static Symbol *createNonrelocSymbol(std::string const &symName, bool numeric) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
		purgedSymbols.erase(sym->name);
	} else if (sym->isDefined()) {
		alreadyDefinedError(*sym, nullptr);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	} else if (!numeric) {
		// The symbol has already been referenced, but it's not allowed
		error("'%s' already referenced at ", symName.c_str());
		dumpFilename(*sym);
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	}

	return sym;
}

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
		alreadyDefinedError(*sym, "non-EQU");
		return nullptr;
	} else if (sym->isBuiltin) {
		redefinedError(*sym);
		return nullptr;
	}

	updateSymbolFilename(*sym);
	sym->type = SYM_EQU;
	sym->data = value;

	return sym;
}

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
		if (sym->isDefined()) {
			alreadyDefinedError(*sym, "non-EQUS");
		} else {
			error("'%s' already referenced at ", symName.c_str());
			dumpFilename(*sym);
		}
		return nullptr;
	} else if (sym->isBuiltin) {
		redefinedError(*sym);
		return nullptr;
	}

	updateSymbolFilename(*sym);
	sym->data = str;

	return sym;
}

Symbol *sym_AddVar(std::string const &symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
	} else if (sym->isDefined() && sym->type != SYM_VAR) {
		alreadyDefinedError(*sym, sym->type == SYM_LABEL ? "label" : "constant");
		return sym;
	} else {
		updateSymbolFilename(*sym);
	}

	sym->type = SYM_VAR;
	sym->data = value;

	return sym;
}

static Symbol *addLabel(std::string const &symName) {
	assumeAlreadyExpanded(symName);

	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		sym = &createSymbol(symName);
	} else if (sym->isDefined()) {
		alreadyDefinedError(*sym, nullptr);
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

Symbol *sym_AddLocalLabel(std::string const &symName) {
	// The symbol name should be local, qualified or not
	assume(symName.find('.') != std::string::npos);

	Symbol *sym = addLabel(isAutoScoped(symName) ? globalScope->name + symName : symName);

	if (sym)
		localScope = sym;

	return sym;
}

Symbol *sym_AddLabel(std::string const &symName) {
	// The symbol name should be global
	assume(symName.find('.') == std::string::npos);

	Symbol *sym = addLabel(symName);

	if (sym) {
		globalScope = sym;
		// A new global scope resets the local scope
		localScope = nullptr;
	}

	return sym;
}

static uint32_t anonLabelID = 0;

Symbol *sym_AddAnonLabel() {
	if (anonLabelID == UINT32_MAX) {
		error("Only %" PRIu32 " anonymous labels can be created!", anonLabelID);
		return nullptr;
	}

	std::string anon = sym_MakeAnonLabelName(0, true); // The direction is important!
	anonLabelID++;
	return addLabel(anon);
}

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
		sym = &createSymbol(isAutoScoped(symName) ? globalScope->name + symName : symName);
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
	PCSymbol->data = PCCallback;
	PCSymbol->isBuiltin = true;

	NARGSymbol = &createSymbol("_NARG"s);
	NARGSymbol->type = SYM_EQU;
	NARGSymbol->data = NARGCallback;
	NARGSymbol->isBuiltin = true;

	globalScopeSymbol = &createSymbol("."s);
	globalScopeSymbol->type = SYM_EQUS;
	globalScopeSymbol->data = globalScopeCallback;
	globalScopeSymbol->isBuiltin = true;

	localScopeSymbol = &createSymbol(".."s);
	localScopeSymbol->type = SYM_EQUS;
	localScopeSymbol->data = localScopeCallback;
	localScopeSymbol->isBuiltin = true;

	RSSymbol = sym_AddVar("_RS"s, 0);
	RSSymbol->isBuiltin = true;

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
