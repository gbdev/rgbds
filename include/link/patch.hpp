// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_PATCH_HPP
#define RGBDS_LINK_PATCH_HPP

#include <string>
#include <vector>

#include "link/section.hpp"

struct Symbol;

struct Assertion {
	Expression rpn;
	uint32_t offset;
	AssertionType type;
	std::string message;
	// This would be redundant with `rpn.pcSection->fileSymbols`,
	// but `rpn.pcSection` is sometimes `nullptr`!
	std::vector<Symbol> *fileSymbols;
};

Assertion &patch_AddAssertion();

// Checks all assertions
void patch_CheckAssertions();

// Applies all SECTIONs' patches to them
void patch_ApplyPatches();

#endif // RGBDS_LINK_PATCH_HPP
