// SPDX-License-Identifier: MIT

#include "asm/symbol.hpp"

#include <algorithm>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>

#include "diagnostics.hpp"
#include "helpers.hpp" // assume
#include "util.hpp"
#include "version.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/macro.hpp"
#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/warning.hpp"

using namespace std::literals;

static std::unordered_map<std::string, Symbol> symbols;
static std::unordered_set<std::string> purgedSymbols;

static Symbol const *globalScope = nullptr; // Current section's global label scope
static Symbol const *localScope = nullptr;  // Current section's local label scope

static Symbol *PCSymbol;
static Symbol *NARGSymbol;
static Symbol *globalScopeSymbol;
static Symbol *localScopeSymbol;
static Symbol *RSSymbol;

static char savedTIME[256];
static char savedDATE[256];
static char savedTIMESTAMP_ISO8601_LOCAL[256];
static char savedTIMESTAMP_ISO8601_UTC[256];

bool sym_IsPC(Symbol const *sym) {
	return sym == PCSymbol;
}

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols) {
		callback(it.second);
	}
}

static int32_t NARGCallback() {
	if (MacroArgs const *macroArgs = fstk_GetCurrentMacroArgs(); macroArgs) {
		return macroArgs->nbArgs();
	} else {
		error("`_NARG` has no value outside of a macro");
		return 0;
	}
}

static std::shared_ptr<std::string> globalScopeCallback() {
	if (!globalScope) {
		error("`.` has no value outside of a label scope");
		return std::make_shared<std::string>("");
	}
	return std::make_shared<std::string>(globalScope->name);
}

static std::shared_ptr<std::string> localScopeCallback() {
	if (!localScope) {
		error("`..` has no value outside of a local label scope");
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
	assume(std::holds_alternative<ContentSpan>(data));
	return std::get<ContentSpan>(data);
}

std::shared_ptr<std::string> Symbol::getEqus() const {
	assume(
	    std::holds_alternative<std::shared_ptr<std::string>>(data)
	    || std::holds_alternative<std::shared_ptr<std::string> (*)()>(data)
	);
	if (auto *callback = std::get_if<std::shared_ptr<std::string> (*)()>(&data); callback) {
		return (*callback)();
	}
	return std::get<std::shared_ptr<std::string>>(data);
}

// Meant to be called last in an `errorNoTrace` callback
static void printBacktraces(Symbol const &sym) {
	putc('\n', stderr);
	fstk_TraceCurrent();
	fputs("    and also:\n", stderr);
	if (sym.src) {
		sym.src->printBacktrace(sym.fileLine);
	} else {
		fprintf(stderr, "    at <%s>\n", sym.isBuiltin ? "builtin" : "command-line");
	}
}

static void updateSymbolFilename(Symbol &sym) {
	std::shared_ptr<FileStackNode> oldSrc = std::move(sym.src);
	sym.src = fstk_GetFileStack();
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0;

	// If the old node was registered, ensure the new one is too
	if (oldSrc && oldSrc->ID != UINT32_MAX) {
		out_RegisterNode(sym.src);
	}
}

static bool isValidIdentifier(std::string const &s) {
	return !s.empty() && startsIdentifier(s[0])
	       && std::all_of(s.begin() + 1, s.end(), [](char c) { return continuesIdentifier(c); });
}

static void alreadyDefinedError(Symbol const &sym, char const *asType) {
	auto suggestion = [&]() {
		std::string s;
		if (auto const &contents = sym.type == SYM_EQUS ? sym.getEqus() : nullptr;
		    contents && isValidIdentifier(*contents)) {
			s.append(" (should it be {interpolated} to define its contents \"");
			s.append(*contents);
			s.append("\"?)");
		}
		return s;
	};

	if (sym.isBuiltin) {
		if (sym_FindScopedValidSymbol(sym.name)) {
			if (std::string s = suggestion(); asType) {
				error("`%s` already defined as built-in %s%s", sym.name.c_str(), asType, s.c_str());
			} else {
				error("`%s` already defined as built-in%s", sym.name.c_str(), s.c_str());
			}
		} else {
			// `DEF()` would return false, so we should not claim the symbol is already defined,
			// nor suggest to interpolate it
			if (asType) {
				error("`%s` is reserved for a built-in %s symbol", sym.name.c_str(), asType);
			} else {
				error("`%s` is reserved for a built-in symbol", sym.name.c_str());
			}
		}
	} else {
		errorNoTrace([&]() {
			fprintf(stderr, "`%s` already defined", sym.name.c_str());
			if (asType) {
				fprintf(stderr, " as %s", asType);
			}
			fputs(suggestion().c_str(), stderr);
			printBacktraces(sym);
		});
	}
}

static void redefinedError(Symbol const &sym) {
	assume(sym.isBuiltin);
	if (sym_FindScopedValidSymbol(sym.name)) {
		error("Built-in symbol `%s` cannot be redefined", sym.name.c_str());
	} else {
		// `DEF()` would return false, so we should not imply the symbol is already defined
		error("`%s` is reserved for a built-in symbol", sym.name.c_str());
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
	sym.isBuiltin = false;
	sym.isExported = false;
	sym.isQuiet = false;
	sym.section = nullptr;
	sym.src = fstk_GetFileStack();
	sym.fileLine = sym.src ? lexer_GetLineNo() : 0;
	sym.ID = UINT32_MAX;
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
	if (dotPos == std::string::npos) {
		return false;
	}

	// Label scopes `.` and `..` are the only nonlocal identifiers that start with a dot
	if (dotPos == 0 && symName.find_first_not_of('.') == symName.npos) {
		return false;
	}

	// Check for nothing after the dot
	if (dotPos == symName.length() - 1) {
		fatal("`%s` is a nonsensical reference to an empty local label", symName.c_str());
	}

	// Check for more than one dot
	if (symName.find('.', dotPos + 1) != std::string::npos) {
		fatal("`%s` is a nonsensical reference to a nested local label", symName.c_str());
	}

	// Check for already-qualified local label
	if (dotPos > 0) {
		return false;
	}

	// Check for unqualifiable local label
	if (!globalScope) {
		fatal("Unqualified local label `%s` in main scope", symName.c_str());
	}

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
		if (sym_IsPurgedScoped(symName)) {
			error("Undefined symbol `%s` was already purged", symName.c_str());
		} else {
			error("Undefined symbol `%s`", symName.c_str());
		}
	} else if (sym->isBuiltin) {
		error("Built-in symbol `%s` cannot be purged", symName.c_str());
	} else if (sym->ID != UINT32_MAX) {
		error("Symbol `%s` is referenced and thus cannot be purged", symName.c_str());
	} else {
		if (sym->isExported) {
			warning(WARNING_PURGE_1, "Purging an exported symbol `%s`", symName.c_str());
		} else if (sym->isLabel()) {
			warning(WARNING_PURGE_2, "Purging a label `%s`", symName.c_str());
		}
		// Do not keep a reference to the label after purging it
		if (sym == globalScope) {
			globalScope = nullptr;
		}
		if (sym == localScope) {
			localScope = nullptr;
		}
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
	if (isConstant()) {
		return getValue();
	}

	if (sym_IsPC(this)) {
		assume(getSection()); // There's no way to reach here from outside of a section
		error("PC does not have a constant value; the current section is not fixed");
	} else {
		error("`%s` does not have a constant value", name.c_str());
	}
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
		errorNoTrace([&]() {
			fprintf(stderr, "`%s` already referenced", symName.c_str());
			printBacktraces(*sym);
		});
		return nullptr; // Don't allow overriding the symbol, that'd be bad!
	}

	return sym;
}

Symbol *sym_AddEqu(std::string const &symName, int32_t value) {
	Symbol *sym = createNonrelocSymbol(symName, true);

	if (!sym) {
		return nullptr;
	}

	sym->type = SYM_EQU;
	sym->data = value;

	return sym;
}

Symbol *sym_RedefEqu(std::string const &symName, int32_t value) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		return sym_AddEqu(symName, value);
	}

	if (sym->isDefined() && sym->type != SYM_EQU) {
		alreadyDefinedError(*sym, "non-`EQU`");
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

	if (!sym) {
		return nullptr;
	}

	sym->type = SYM_EQUS;
	sym->data = str;
	return sym;
}

Symbol *sym_RedefString(std::string const &symName, std::shared_ptr<std::string> str) {
	Symbol *sym = sym_FindExactSymbol(symName);

	if (!sym) {
		return sym_AddString(symName, str);
	}

	if (sym->type != SYM_EQUS) {
		if (sym->isDefined()) {
			alreadyDefinedError(*sym, "non-`EQUS`");
		} else {
			errorNoTrace([&]() {
				fprintf(stderr, "`%s` already referenced", symName.c_str());
				printBacktraces(*sym);
			});
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
	sym->data = static_cast<int32_t>(sect_GetSymbolOffset());
	// Don't export anonymous labels
	if (options.exportAll && !symName.starts_with('!')) {
		sym->isExported = true;
	}
	sym->section = sect_GetSymbolSection();

	if (sym && !sym->section) {
		error("Label `%s` created outside of a `SECTION`", symName.c_str());
	}

	return sym;
}

Symbol *sym_AddLocalLabel(std::string const &symName) {
	// The symbol name should be local, qualified or not
	assume(symName.find('.') != std::string::npos);

	Symbol *sym = addLabel(isAutoScoped(symName) ? globalScope->name + symName : symName);

	if (sym) {
		localScope = sym;
	}

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
		// LCOV_EXCL_START
		error("Only %" PRIu32 " anonymous labels can be created!", anonLabelID);
		return nullptr;
		// LCOV_EXCL_STOP
	}

	std::string anon = sym_MakeAnonLabelName(0, true); // The direction is important!
	++anonLabelID;
	return addLabel(anon);
}

std::string sym_MakeAnonLabelName(uint32_t ofs, bool neg) {
	uint32_t id = 0;

	if (neg) {
		if (ofs > anonLabelID) {
			error(
			    "Reference to anonymous label %" PRIu32 " before, when only %" PRIu32
			    " ha%s been created so far",
			    ofs,
			    anonLabelID,
			    anonLabelID == 1 ? "s" : "ve"
			);
		} else {
			id = anonLabelID - ofs;
		}
	} else {
		// We're referencing symbols that haven't been created yet...
		if (--ofs > UINT32_MAX - anonLabelID) {
			// LCOV_EXCL_START
			error(
			    "Reference to anonymous label %" PRIu32 " after, when only %" PRIu32
			    " can still be created",
			    ofs + 1,
			    UINT32_MAX - anonLabelID
			);
		} else {
			// LCOV_EXCL_STOP
			id = anonLabelID + ofs;
		}
	}

	return "!"s + std::to_string(id);
}

void sym_Export(std::string const &symName) {
	if (symName.starts_with('!')) {
		// LCOV_EXCL_START
		// The parser does not accept anonymous labels for an `EXPORT` directive
		error("Anonymous labels cannot be exported");
		return;
		// LCOV_EXCL_STOP
	}

	Symbol *sym = sym_FindScopedSymbol(symName);

	// If the symbol doesn't exist, create a ref that can be purged
	if (!sym) {
		sym = sym_Ref(symName);
	}
	sym->isExported = true;
}

Symbol *sym_AddMacro(
    std::string const &symName, int32_t defLineNo, ContentSpan const &span, bool isQuiet
) {
	Symbol *sym = createNonrelocSymbol(symName, false);

	if (!sym) {
		return nullptr;
	}

	sym->type = SYM_MACRO;
	sym->data = span;
	sym->isQuiet = isQuiet;

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

	// LCOV_EXCL_START
	if (now == static_cast<time_t>(-1)) {
		warnx("Failed to determine current time: %s", strerror(errno));
		// Fall back by pretending we are at the Epoch
		now = 0;
	}
	// LCOV_EXCL_STOP

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

	Symbol *timeSymbol = &createSymbol("__TIME__"s);
	timeSymbol->type = SYM_EQUS;
	timeSymbol->data = []() {
		warning(WARNING_OBSOLETE, "`__TIME__` is deprecated; use `__ISO_8601_LOCAL__`");
		return std::make_shared<std::string>(savedTIME);
	};
	timeSymbol->isBuiltin = true;

	Symbol *dateSymbol = &createSymbol("__DATE__"s);
	dateSymbol->type = SYM_EQUS;
	dateSymbol->data = []() {
		warning(WARNING_OBSOLETE, "`__DATE__` is deprecated; use `__ISO_8601_LOCAL__`");
		return std::make_shared<std::string>(savedDATE);
	};
	dateSymbol->isBuiltin = true;

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
