/* SPDX-License-Identifier: MIT */

// Applying patches to SECTIONs
#ifndef RGBDS_LINK_PATCH_H
#define RGBDS_LINK_PATCH_H

#include <deque>
#include <stdint.h>

#include "link/section.hpp"

#include "linkdefs.hpp"

struct Assertion {
	struct Patch patch; // Also used for its `.type`
	char *message;
	// This would be redundant with `.section->fileSymbols`... but `section` is sometimes NULL!
	struct Symbol **fileSymbols;
};

/*
 * Checks all assertions
 * @return true if assertion failed
 */
void patch_CheckAssertions(std::deque<struct Assertion> &assertions);

/*
 * Applies all SECTIONs' patches to them
 */
void patch_ApplyPatches(void);

#endif // RGBDS_LINK_PATCH_H
