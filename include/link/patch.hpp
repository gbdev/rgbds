// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_PATCH_HPP
#define RGBDS_LINK_PATCH_HPP

#include <string>
#include <vector>

#include "link/section.hpp"

struct Symbol;

struct Assertion {
	Patch patch; // Also used for its `.type`
	std::string message;
	// This would be redundant with `patch.pcSection->fileSymbols`, but `section` is sometimes
	// `nullptr`!
	std::vector<Symbol> *fileSymbols;
};

Assertion &patch_AddAssertion();

// Checks all assertions
void patch_CheckAssertions();

// Applies all SECTIONs' patches to them
void patch_ApplyPatches();

#endif // RGBDS_LINK_PATCH_HPP
