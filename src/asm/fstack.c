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

#ifdef LEXER_DEBUG
  #define dbgPrint(...) fprintf(stderr, "[lexer] " __VA_ARGS__)
#else
  #define dbgPrint(...)
#endif

struct Context {
	struct Context *parent;
	struct FileStackNode *fileInfo;
	struct LexerState *lexerState;
	uint32_t uniqueID;
	struct MacroArgs *macroArgs; /* Macro args are *saved* here */
	uint32_t nbReptIters;
};

static struct Context *contextStack;
static size_t contextDepth = 0;
#define DEFAULT_MAX_DEPTH 64
size_t nMaxRecursionDepth;

static unsigned int nbIncPaths = 0;
static char const *includePaths[MAXINCPATHS];

char const *dumpNodeAndParents(struct FileStackNode const *node)
{
	char const *name;

	if (node->type == NODE_REPT) {
		assert(node->parent); /* REPT nodes should always have a parent */
		struct FileStackReptNode const *reptInfo = (struct FileStackReptNode const *)node;

		name = dumpNodeAndParents(node->parent);
		fprintf(stderr, "(%" PRIu32 ") -> %s", node->lineNo, name);
		for (uint32_t i = reptInfo->reptDepth; i--; )
			fprintf(stderr, "::REPT~%" PRIu32, reptInfo->iters[i]);
	} else {
		name = ((struct FileStackNamedNode const *)node)->name;
		if (node->parent) {
			dumpNodeAndParents(node->parent);
			fprintf(stderr, "(%" PRIu32 ") -> %s", node->lineNo, name);
		} else {
			fputs(name, stderr);
		}
	}
	return name;
}

void fstk_Dump(struct FileStackNode const *node, uint32_t lineNo)
{
	dumpNodeAndParents(node);
	fprintf(stderr, "(%" PRIu32 ")", lineNo);
}

void fstk_DumpCurrent(void)
{
	if (!contextStack) {
		fputs("at top level", stderr);
		return;
	}
	fstk_Dump(contextStack->fileInfo, lexer_GetLineNo());
}

struct FileStackNode *fstk_GetFileStack(void)
{
	struct FileStackNode *node = contextStack->fileInfo;

	/* Mark node and all of its parents as referenced if not already so they don't get freed */
	while (node && !node->referenced) {
		node->ID = -1;
		node->referenced = true;
		node = node->parent;
	}
	return contextStack->fileInfo;
}

char const *fstk_GetFileName(void)
{
	/* Iterating via the nodes themselves skips nested REPTs */
	struct FileStackNode const *node = contextStack->fileInfo;

	while (node->type != NODE_FILE)
		node = node->parent;
	return ((struct FileStackNamedNode const *)node)->name;
}

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
	if (contextStack->fileInfo->type == NODE_REPT) { /* The context is a REPT block, which may loop */
		struct FileStackReptNode *fileInfo = (struct FileStackReptNode *)contextStack->fileInfo;

		/* If the node is referenced, we can't edit it; duplicate it */
		if (contextStack->fileInfo->referenced) {
			size_t size = sizeof(*fileInfo) + sizeof(fileInfo->iters[0]) * fileInfo->reptDepth;
			struct FileStackReptNode *copy = malloc(size);

			if (!copy)
				fatalerror("Failed to duplicate REPT file node: %s\n", strerror(errno));
			/* Copy all info but the referencing */
			memcpy(copy, fileInfo, size);
			copy->node.next = NULL;
			copy->node.referenced = false;

			fileInfo = copy;
			contextStack->fileInfo = (struct FileStackNode *)fileInfo;
		}

		fileInfo->iters[0]++;
		/* If this wasn't the last iteration, wrap instead of popping */
		if (fileInfo->iters[0] <= contextStack->nbReptIters) {
			lexer_RestartRept(contextStack->fileInfo->lineNo);
			contextStack->uniqueID = macro_UseNewUniqueID();
			return false;
		}
	} else if (!contextStack->parent) {
		return true;
	}
	dbgPrint("Popping context\n");

	struct Context *context = contextStack;

	contextStack = contextStack->parent;
	contextDepth--;

	lexer_DeleteState(context->lexerState);
	/* Restore args if a macro (not REPT) saved them */
	if (context->fileInfo->type == NODE_MACRO) {
		dbgPrint("Restoring macro args %p\n", contextStack->macroArgs);
		macro_UseNewArgs(contextStack->macroArgs);
	}
	/* Free the file stack node */
	if (!context->fileInfo->referenced)
		free(context->fileInfo);
	/* Free the entry and make its parent the current entry */
	free(context);

	lexer_SetState(contextStack->lexerState);
	macro_SetUniqueID(contextStack->uniqueID);
	return false;
}

/*
 * Make sure not to switch the lexer state before calling this, so the saved line no is correct
 * BE CAREFUL!! This modifies the file stack directly, you should have set up the file info first
 */
static void newContext(struct FileStackNode *fileInfo)
{
	if (++contextDepth >= nMaxRecursionDepth)
		fatalerror("Recursion limit (%zu) exceeded\n", nMaxRecursionDepth);
	struct Context *context = malloc(sizeof(*context));

	if (!context)
		fatalerror("Failed to allocate memory for new context: %s\n", strerror(errno));
	fileInfo->parent = contextStack->fileInfo;
	fileInfo->lineNo = 0; /* Init to a default value, see struct definition for info */
	fileInfo->referenced = false;
	fileInfo->lineNo = lexer_GetLineNo();
	context->fileInfo = fileInfo;
	/*
	 * Link new entry to its parent so it's reachable later
	 * ERRORS SHOULD NOT OCCUR AFTER THIS!!
	 */
	context->parent = contextStack;
	contextStack = context;

}

void fstk_RunInclude(char const *path)
{
	dbgPrint("Including path \"%s\"\n", path);

	char *fullPath = NULL;
	size_t size = 0;

	if (!fstk_FindFile(path, &fullPath, &size)) {
		free(fullPath);
		if (oGeneratedMissingIncludes)
			oFailedOnMissingInclude = true;
		else
			error("Unable to open included file '%s': %s\n", path, strerror(errno));
		return;
	}
	dbgPrint("Full path: \"%s\"\n", fullPath);

	struct FileStackNamedNode *fileInfo = malloc(sizeof(*fileInfo) + size);

	if (!fileInfo) {
		error("Failed to alloc file info for INCLUDE: %s\n", strerror(errno));
		return;
	}
	fileInfo->node.type = NODE_FILE;
	strcpy(fileInfo->name, fullPath);
	free(fullPath);

	newContext((struct FileStackNode *)fileInfo);
	contextStack->lexerState = lexer_OpenFile(fileInfo->name);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for file include\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	/* We're back at top-level, so most things are reset */
	contextStack->uniqueID = 0;
	macro_SetUniqueID(0);
}

void fstk_RunMacro(char const *macroName, struct MacroArgs *args)
{
	dbgPrint("Running macro \"%s\"\n", macroName);

	struct Symbol *macro = sym_FindSymbol(macroName);

	if (!macro) {
		error("Macro \"%s\" not defined\n", macroName);
		return;
	}
	if (macro->type != SYM_MACRO) {
		error("\"%s\" is not a macro\n", macroName);
		return;
	}
	contextStack->macroArgs = macro_GetCurrentArgs();

	/* Compute total length of this node's name: <base name>::<macro> */
	size_t reptNameLen = 0;
	struct FileStackNode const *node = macro->src;

	if (node->type == NODE_REPT) {
		struct FileStackReptNode const *reptNode = (struct FileStackReptNode const *)node;

		/* 4294967295 = 2^32 - 1, aka UINT32_MAX */
		reptNameLen += reptNode->reptDepth * strlen("::REPT~4294967295");
		/* Look for next named node */
		do {
			node = node->parent;
		} while (node->type == NODE_REPT);
	}
	struct FileStackNamedNode const *baseNode = (struct FileStackNamedNode const *)node;
	size_t baseLen = strlen(baseNode->name);
	size_t macroNameLen = strlen(macro->name);
	struct FileStackNamedNode *fileInfo = malloc(sizeof(*fileInfo) + baseLen
						     + reptNameLen + 2 + macroNameLen + 1);

	if (!fileInfo) {
		error("Failed to alloc file info for \"%s\": %s\n", macro->name, strerror(errno));
		return;
	}
	fileInfo->node.type = NODE_MACRO;
	/* Print the name... */
	char *dest = fileInfo->name;

	memcpy(dest, baseNode->name, baseLen);
	dest += baseLen;
	if (node->type == NODE_REPT) {
		struct FileStackReptNode const *reptNode = (struct FileStackReptNode const *)node;

		for (uint32_t i = reptNode->reptDepth; i--; ) {
			int nbChars = sprintf(dest, "::REPT~%" PRIu32, reptNode->iters[i]);

			if (nbChars < 0)
				fatalerror("Failed to write macro invocation info: %s\n",
					   strerror(errno));
			dest += nbChars;
		}
	}
	*dest++ = ':';
	*dest++ = ':';
	memcpy(dest, macro->name, macroNameLen + 1);

	newContext((struct FileStackNode *)fileInfo);
	/* Line minus 1 because buffer begins with a newline */
	contextStack->lexerState = lexer_OpenFileView(macro->macro, macro->macroSize,
						      macro->fileLine - 1);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for macro invocation\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	contextStack->uniqueID = macro_UseNewUniqueID();
	macro_UseNewArgs(args);
}

void fstk_RunRept(uint32_t count, int32_t reptLineNo, char *body, size_t size)
{
	dbgPrint("Running REPT(%" PRIu32 ")\n", count);
	if (count == 0)
		return;

	uint32_t reptDepth = contextStack->fileInfo->type == NODE_REPT
				? ((struct FileStackReptNode *)contextStack->fileInfo)->reptDepth
				: 0;
	struct FileStackReptNode *fileInfo = malloc(sizeof(*fileInfo)
						    + (reptDepth + 1) * sizeof(fileInfo->iters[0]));

	if (!fileInfo) {
		error("Failed to alloc file info for REPT: %s\n", strerror(errno));
		return;
	}
	fileInfo->node.type = NODE_REPT;
	fileInfo->reptDepth = reptDepth + 1;
	fileInfo->iters[0] = 1;
	if (reptDepth)
		/* Copy all parent iter counts */
		memcpy(&fileInfo->iters[1],
		       ((struct FileStackReptNode *)contextStack->fileInfo)->iters,
		       reptDepth * sizeof(fileInfo->iters[0]));

	newContext((struct FileStackNode *)fileInfo);
	/* Correct our line number, which currently points to the `ENDR` line */
	contextStack->fileInfo->lineNo = reptLineNo;

	contextStack->lexerState = lexer_OpenFileView(body, size, reptLineNo);
	if (!contextStack->lexerState)
		fatalerror("Failed to set up lexer for rept block\n");
	lexer_SetStateAtEOL(contextStack->lexerState);
	contextStack->uniqueID = macro_UseNewUniqueID();
	contextStack->nbReptIters = count;

}

void fstk_Init(char const *mainPath, size_t maxRecursionDepth)
{
	struct LexerState *state = lexer_OpenFile(mainPath);

	if (!state)
		fatalerror("Failed to open main file!\n");
	lexer_SetState(state);
	char const *fileName = lexer_GetFileName();
	size_t len = strlen(fileName);
	struct Context *context = malloc(sizeof(*contextStack));
	struct FileStackNamedNode *fileInfo = malloc(sizeof(*fileInfo) + len + 1);

	if (!context)
		fatalerror("Failed to allocate memory for main context: %s\n", strerror(errno));
	if (!fileInfo)
		fatalerror("Failed to allocate memory for main file info: %s\n", strerror(errno));

	context->fileInfo = (struct FileStackNode *)fileInfo;
	/* lineNo and reptIter are unused on the top-level context */
	context->fileInfo->parent = NULL;
	context->fileInfo->referenced = false;
	context->fileInfo->type = NODE_FILE;
	memcpy(fileInfo->name, fileName, len + 1);

	context->parent = NULL;
	context->lexerState = state;
	context->uniqueID = 0;
	macro_SetUniqueID(0);
	context->nbReptIters = 0;

	/* Now that it's set up properly, register the context */
	contextStack = context;

	/*
	 * Check that max recursion depth won't allow overflowing node `malloc`s
	 * This assumes that the rept node is larger
	 */
#define DEPTH_LIMIT ((SIZE_MAX - sizeof(struct FileStackReptNode)) / sizeof(uint32_t))
	if (maxRecursionDepth > DEPTH_LIMIT) {
		error("Recursion depth may not be higher than %zu, defaulting to "
		      EXPAND_AND_STR(DEFAULT_MAX_DEPTH) "\n", DEPTH_LIMIT);
		nMaxRecursionDepth = DEFAULT_MAX_DEPTH;
	} else {
		nMaxRecursionDepth = maxRecursionDepth;
	}
	/* Make sure that the default of 64 is OK, though */
	assert(DEPTH_LIMIT >= DEFAULT_MAX_DEPTH);
#undef DEPTH_LIMIT
}
