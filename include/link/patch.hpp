/* SPDX-License-Identifier: MIT */

// Applying patches to SECTIONs
#ifndef RGBDS_LINK_PATCH_H
#define RGBDS_LINK_PATCH_H

#include <deque>
#include <stdint.h>
#include <vector>

#include "link/section.hpp"

#include "linkdefs.hpp"

struct Assertion {
	Patch patch; // Also used for its `.type`
	std::string message;
	// This would be redundant with `.section->fileSymbols`, but `section` is sometimes `nullptr`!
	std::vector<Symbol> *fileSymbols;
};

/*
 * Checks all assertions
 * @return true if assertion failed
 */
void patch_CheckAssertions(std::deque<Assertion> &assertions);

/*
 * Applies all SECTIONs' patches to them
 */
void patch_ApplyPatches(void);

#endif // RGBDS_LINK_PATCH_H
