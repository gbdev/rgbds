// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_PATCH_HPP
#define RGBDS_LINK_PATCH_HPP

#include <vector>

struct Patch;
struct Section;
struct Symbol;

// Checks all assertions
void patch_CheckAssertions();

// Applies all SECTIONs' patches to them
void patch_ApplyPatches();

// Executes a callback on all sections referenced by a patch's expression
void patch_FindReferencedSections(
    Patch const &patch, void (*callback)(Section &), std::vector<Symbol> const &fileSymbols
);

#endif // RGBDS_LINK_PATCH_HPP
