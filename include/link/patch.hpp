/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_PATCH_H
#define RGBDS_LINK_PATCH_H

#include <deque>
#include <stdint.h>
#include <vector>

#include "linkdefs.hpp"

#include "link/section.hpp"

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
void patch_ApplyPatches();

#endif // RGBDS_LINK_PATCH_H
