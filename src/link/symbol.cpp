/* SPDX-License-Identifier: MIT */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "link/object.hpp"
#include "link/symbol.hpp"
#include "link/main.hpp"

#include "error.hpp"
#include "hashmap.hpp"

HashMap symbols;

struct ForEachSymbolArg {
	void (*callback)(struct Symbol *symbol, void *arg);
	void *arg;
};

static void forEach(void *symbol, void *arg)
{
	struct ForEachSymbolArg *callbackArg = (struct ForEachSymbolArg *)arg;

	callbackArg->callback((struct Symbol *)symbol, callbackArg->arg);
}

void sym_ForEach(void (*callback)(struct Symbol *, void *), void *arg)
{
	struct ForEachSymbolArg callbackArg = { .callback = callback, .arg = arg };

	hash_ForEach(symbols, forEach, &callbackArg);
}

void sym_AddSymbol(struct Symbol *symbol)
{
	// Check if the symbol already exists
	struct Symbol *other = (struct Symbol *)hash_GetElement(symbols, symbol->name);

	if (other) {
		fprintf(stderr, "error: \"%s\" both in %s from ", symbol->name, symbol->objFileName);
		dumpFileStack(symbol->src);
		fprintf(stderr, "(%" PRIu32 ") and in %s from ",
			symbol->lineNo, other->objFileName);
		dumpFileStack(other->src);
		fprintf(stderr, "(%" PRIu32 ")\n", other->lineNo);
		exit(1);
	}

	// If not, add it
	hash_AddElement(symbols, symbol->name, symbol);
}

struct Symbol *sym_GetSymbol(char const *name)
{
	return (struct Symbol *)hash_GetElement(symbols, name);
}

void sym_CleanupSymbols(void)
{
	hash_EmptyMap(symbols);
}
