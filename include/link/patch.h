/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Applying patches to SECTIONs */
#ifndef RGBDS_LINK_PATCH_H
#define RGBDS_LINK_PATCH_H

#include <stdbool.h>
#include <stdint.h>

#include "link/section.h"

#include "linkdefs.h"

struct Symbol;

struct Assertion {
	struct Patch patch;
	// enum AssertionType type; The `patch`'s field is instead re-used
	char *message;
	/*
	 * This would be redundant with `.section->fileSymbols`... but
	 * `section` is sometimes NULL!
	 */
	struct Symbol **fileSymbols;

	struct Assertion *next;
};

/**
 * Checks all assertions
 * @param assertion The first assertion to check (in a linked list)
 * @return true if assertion failed
 */
void patch_CheckAssertions(struct Assertion *assertion);

/**
 * Applies all SECTIONs' patches to them
 */
void patch_ApplyPatches(void);

/**
 * Executes a callback on all sections referenced by a patch's expression
 * @param patch The patch to scan the expression of
 */
void patch_FindRefdSections(struct Patch const *patch, void (*callback)(struct Section *),
			    struct Symbol const * const *fileSymbols);

/**
 * Properly deletes a patch object
 * @param patch The patch to be deleted
 */
static inline void patch_DeletePatch(struct Patch *patch)
{
	free(patch->rpnExpression);
}

#endif /* RGBDS_LINK_PATCH_H */
