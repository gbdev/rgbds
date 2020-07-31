/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "asm/fstack.h"
#include "asm/macro.h"
#include "asm/main.h"
#include "asm/symbol.h"
#include "asm/warning.h"
#include "platform.h" /* S_ISDIR (stat macro) */

struct Context {
	struct Context *parent;
	struct Context *child;
	struct LexerState *lexerState;
	uint32_t uniqueID;
	char *fileName;
	uint32_t lineNo; /* Line number at which the context was EXITED */
	struct Symbol const *macro;
	uint32_t nbReptIters; /* If zero, this isn't a REPT block */
	size_t reptDepth;
	uint32_t reptIters[];
};

static struct Context *contextStack;
static struct Context *topLevelContext;
static unsigned int contextDepth = 0;
unsigned int nMaxRecursionDepth;

static unsigned int nbIncPaths = 0;
static char const *includePaths[MAXINCPATHS];

void fstk_AddIncludePath(char const *path)
{
	if (path[0] == '\0')
		return;
	if (nbIncPaths >= MAXINCPATHS) {
		error("Too many include directories passed from command line\n");
		return;
	}
	size_t len = strlen(path);
	size_t allocSize = len + (path[len - 1] != '/') + 1;
	char *str = malloc(allocSize);

	if (!str) {
		/* Attempt to continue without that path */
		error("Failed to allocate new include path: %s\n", strerror(errno));
		return;
	}
	memcpy(str, path, len);
	char *end = str + len - 1;

	if (*end++ != '/')
		*end++ = '/';
	*end = '\0';
	includePaths[nbIncPaths++] = str;
}

static void printDep(char const *path)
{
	if (dependfile) {
		fprintf(dependfile, "%s: %s\n", tzTargetFileName, path);
		if (oGeneratePhonyDeps)
			fprintf(dependfile, "%s:\n", path);
	}
}

static bool isPathValid(char const *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) != 0)
		return false;

	/* Reject directories */
	return !S_ISDIR(statbuf.st_mode);
}

bool fstk_FindFile(char const *path, char **fullPath, size_t *size)
{
	if (!*size) {
		*size = 64; /* This is arbitrary, really */
		*fullPath = realloc(*fullPath, *size);
		if (!*fullPath)
			error("realloc error during include path search: %s\n",
			      strerror(errno));
	}

	if (*fullPath) {
		for (size_t i = 0; i <= nbIncPaths; ++i) {
			char const *incPath = i ? includePaths[i - 1] : "";
			int len = snprintf(*fullPath, *size, "%s%s", incPath, path);

			/* Oh how I wish `asnprintf` was standard... */
			if (len >= *size) { /* `len` doesn't include the terminator, `size` does */
				*size = len + 1;
				*fullPath = realloc(*fullPath, *size);
				if (!*fullPath) {
					error("realloc error during include path search: %s\n",
					      strerror(errno));
					break;
				}
				len = sprintf(*fullPath, "%s%s", incPath, path);
			}

			if (len < 0) {
				error("snprintf error during include path search: %s\n",
				      strerror(errno));
			} else if (isPathValid(*fullPath)) {
				printDep(*fullPath);
				return true;
			}
		}
	}

	errno = ENOENT;
	if (oGeneratedMissingIncludes)
		printDep(path);
	return false;
}

bool yywrap(void)
{
	if (contextStack->nbReptIters) { /* The context is a REPT block, which may loop */
		contextStack->reptIters[contextStack->reptDepth - 1]++;
		/* If this wasn't the last iteration, wrap instead of popping */
		if (contextStack->reptIters[contextStack->reptDepth - 1]
								<= contextStack->nbReptIters) {
			lexer_RestartRept(contextStack->parent->lineNo);
			contextStack->uniqueID = macro_UseNewUniqueID();
			return false;
		}
	} else if (!contextStack->parent) {
		return true;
	}
	contextStack = contextStack->parent;
	contextDepth--;

	lexer_DeleteState(contextStack->child->lexerState);
	/* If at top level (= not in macro or in REPT), free the file name */
	if (!contextStack->macro && contextStack->reptIters == 0)
		free(contextStack->child->fileName);
	/* Free the entry and make its parent the current entry */
	free(contextStack->child);

	contextStack->child = NULL;
	lexer_SetState(contextStack->lexerState);
	return false;
}

static void newContext(uint32_t reptDepth)
{
	if (++contextDepth >= nMaxRecursionDepth)
		fatalerror("Recursion limit (%u) exceeded\n", nMaxRecursionDepth);
	contextStack->child = malloc(sizeof(*contextStack->child)
						+ reptDepth * sizeof(contextStack->reptIters[0]));
	if (!contextStack->child)
		fatalerror("Failed to allocate memory for new context: %s\n", strerror(errno));

	contextStack->lineNo = lexer_GetLineNo();
	/* Link new entry to its parent so it's reachable later */
	contextStack->child->parent = contextStack;
	contextStack = contextStack->child;

	contextStack->child = NULL;
	contextStack->reptDepth = reptDepth;
}

void fstk_RunInclude(char const *path)
{
	char *fullPath = NULL;
	size_t size = 0;

	if (!fstk_FindFile(path, &fullPath, &size)) {
		free(fullPath);
		error("Unable to open included file '%s': %s\n", path, strerror(errno));
		return;
	}

	newContext(0);
	contextStack->lexerState = lexer_OpenFile(fullPath);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for file include\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	/* We're back at top-level, so most things are reset */
	contextStack->uniqueID = 0;
	macro_SetUniqueID(0);
	contextStack->fileName = fullPath;
	contextStack->macro = NULL;
	contextStack->nbReptIters = 0;
}

void fstk_RunMacro(char *macroName, struct MacroArgs *args)
{
	struct Symbol *macro = sym_FindSymbol(macroName);

	if (!macro) {
		error("Macro \"%s\" not defined\n", macroName);
		return;
	}
	if (macro->type != SYM_MACRO) {
		error("\"%s\" is not a macro\n", macroName);
		return;
	}
	macro_UseNewArgs(args);

	newContext(0);
	contextStack->lexerState = lexer_OpenFileView(macro->macro,
						      macro->macroSize, macro->fileLine);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for macro invocation\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	contextStack->uniqueID = macro_UseNewUniqueID();
	contextStack->fileName = macro->fileName;
	contextStack->macro = macro;
	contextStack->nbReptIters = 0;
}

void fstk_RunRept(uint32_t count, int32_t nReptLineNo, char *body, size_t size)
{
	uint32_t reptDepth = contextStack->reptDepth;

	newContext(reptDepth + 1);
	contextStack->lexerState = lexer_OpenFileView(body, size, nReptLineNo);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for macro invocation\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	contextStack->uniqueID = macro_UseNewUniqueID();
	contextStack->fileName = contextStack->parent->fileName;
	contextStack->macro = contextStack->parent->macro; /* Inherit */
	contextStack->nbReptIters = count;
	/* Copy all of parent's iters, and add ours */
	if (reptDepth)
		memcpy(contextStack->reptIters, contextStack->parent->reptIters,
		       sizeof(contextStack->reptIters[0]) * reptDepth);
	contextStack->reptIters[reptDepth] = 1;

	/* Correct our parent's line number, which currently points to the `ENDR` line */
	contextStack->parent->lineNo = nReptLineNo;
}

static void printContext(FILE *stream, struct Context const *context)
{
	fprintf(stream, "%s", context->fileName);
	if (context->macro)
		fprintf(stream, "::%s", context->macro->name);
	for (size_t i = 0; i < context->reptDepth; i++)
		fprintf(stream, "::REPT~%" PRIu32, context->reptIters[i]);
	fprintf(stream, "(%" PRId32 ")", context->lineNo);
}

static void dumpToStream(FILE *stream)
{
	struct Context *context = topLevelContext;

	while (context != contextStack) {
		printContext(stream, context);
		fprintf(stream, " -> ");
		context = context->child;
	}
	contextStack->lineNo = lexer_GetLineNo();
	printContext(stream, contextStack);
}

void fstk_Dump(void)
{
	dumpToStream(stderr);
}

char *fstk_DumpToStr(void)
{
	char *str;
	size_t size;
	/* `open_memstream` is specified to always include a '\0' at the end of the buffer! */
	FILE *stream = open_memstream(&str, &size);

	if (!stream)
		fatalerror("Failed to dump file stack to string: %s\n", strerror(errno));
	dumpToStream(stream);
	fclose(stream);
	return str;
}

uint32_t fstk_GetLine(void)
{
	return lexer_GetLineNo();
}

void fstk_Init(char *mainPath, uint32_t maxRecursionDepth)
{
	topLevelContext = malloc(sizeof(*topLevelContext));
	if (!topLevelContext)
		fatalerror("Failed to allocate memory for initial context: %s\n", strerror(errno));
	topLevelContext->parent = NULL;
	topLevelContext->child = NULL;
	topLevelContext->lexerState = lexer_OpenFile(mainPath);
	if (!topLevelContext->lexerState)
		fatalerror("Failed to open main file!\n");
	lexer_SetState(topLevelContext->lexerState);
	topLevelContext->uniqueID = 0;
	macro_SetUniqueID(0);
	topLevelContext->fileName = mainPath;
	topLevelContext->macro = NULL;
	topLevelContext->nbReptIters = 0;
	topLevelContext->reptDepth = 0;

	contextStack = topLevelContext;

#if 0
	if (maxRecursionDepth
			> (SIZE_MAX - sizeof(*contextStack)) / sizeof(contextStack->reptIters[0])) {
#else
	/* If this holds, then GCC raises a warning about the `if` above being dead code */
	static_assert(UINT32_MAX
			<= (SIZE_MAX - sizeof(*contextStack)) / sizeof(contextStack->reptIters[0]));
	if (0) {
#endif
		error("Recursion depth may not be higher than %zu, defaulting to 64\n",
			(SIZE_MAX - sizeof(*contextStack)) / sizeof(contextStack->reptIters[0]));
		nMaxRecursionDepth = 64;
	} else {
		nMaxRecursionDepth = maxRecursionDepth;
	}
}
