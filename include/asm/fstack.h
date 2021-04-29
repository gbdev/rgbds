/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Contains some assembler-wide defines and externs
 */

#ifndef RGBDS_ASM_FSTACK_H
#define RGBDS_ASM_FSTACK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "asm/lexer.h"

#include "types.h"

struct FileStackNode {
	struct FileStackNode *parent; /* Pointer to parent node, for error reporting */
	/* Line at which the parent context was exited; meaningless for the root level */
	uint32_t lineNo;

	struct FileStackNode *next; /* Next node in the output linked list */
	bool referenced; /* If referenced, don't free! */
	uint32_t ID; /* Set only if referenced: ID within the object file, -1 if not output yet */

	enum {
		NODE_REPT,
		NODE_FILE,
		NODE_MACRO,
	} type;
};

struct FileStackReptNode { /* NODE_REPT */
	struct FileStackNode node;
	uint32_t reptDepth;
	/* WARNING: if changing this type, change overflow check in `fstk_Init` */
	uint32_t iters[]; /* REPT iteration counts since last named node, in reverse depth order */
};

struct FileStackNamedNode { /* NODE_FILE, NODE_MACRO */
	struct FileStackNode node;
	struct String *name; /* File name for files, file::macro name for macros */
};

extern size_t maxRecursionDepth;

struct MacroArgs;
struct String;

void fstk_Dump(struct FileStackNode const *node, uint32_t lineNo);
void fstk_DumpCurrent(void);
struct FileStackNode *fstk_GetFileStack(void);
/* The lifetime of the returned chars is until reaching the end of that file */
struct String const *fstk_GetFileName(void);

void fstk_AddIncludePath(char const *s);
/**
 * @param path The user-provided file name
 * @return The full path if the file was found, NULL if no path worked
 */
struct String *fstk_FindFile(struct String const *path);

bool yywrap(void);
void fstk_RunInclude(struct String const *path);
void fstk_RunMacro(struct String const *macroName, struct MacroArgs *args);
void fstk_RunRept(uint32_t count, int32_t reptLineNo, char *body, size_t size);
void fstk_RunFor(struct String *symName, int32_t start, int32_t stop, int32_t step,
		 int32_t reptLineNo, char *body, size_t size);
void fstk_StopRept(void);
bool fstk_Break(void);

void fstk_Init(char const *mainPath, size_t maxDepth);

#endif /* RGBDS_ASM_FSTACK_H */
