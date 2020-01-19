/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "link/symbol.h"
#include "link/main.h"
#include "extern/err.h"
#include "hashmap.h"

HashMap symbols;

struct ForEachArg {
	void (*callback)(struct Symbol *symbol, void *arg);
	void *arg;
};

static void forEach(void *symbol, void *arg)
{
	struct ForEachArg *callbackArg = (struct ForEachArg *)arg;

	callbackArg->callback((struct Symbol *)symbol, callbackArg->arg);
}

void sym_ForEach(void (*callback)(struct Symbol *, void *), void *arg)
{
	struct ForEachArg callbackArg = { .callback = callback, .arg = arg};

	hash_ForEach(symbols, forEach, &callbackArg);
}

void sym_AddSymbol(struct Symbol *symbol)
{
	/* Check if the symbol already exists */
	struct Symbol *other = hash_GetElement(symbols, symbol->name);

	if (other)
		errx(1, "\"%s\" both in %s from %s(%d) and in %s from %s(%d)",
		     symbol->name,
		     symbol->objFileName, symbol->fileName, symbol->lineNo,
		      other->objFileName,  other->fileName,  other->lineNo);

	/* If not, add it */
	bool collided = hash_AddElement(symbols, symbol->name, symbol);

	if (beVerbose && collided)
		warnx("Symbol hashmap collision occurred!");
}

struct Symbol *sym_GetSymbol(char const *name)
{
	return (struct Symbol *)hash_GetElement(symbols, name);
}

void sym_CleanupSymbols(void)
{
	hash_EmptyMap(symbols);
}
