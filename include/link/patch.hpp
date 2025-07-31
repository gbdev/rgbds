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

// Executes a callback on all sections referenced by expressions in a section's patches
void patch_FindSectionReferencedSections(Section &section, void (*callback)(Section &));

// Executes a callback on all sections referenced by expressions in assertions' patches
void patch_FindAssertionReferencedSections(void (*callback)(Section &));

#endif // RGBDS_LINK_PATCH_HPP
