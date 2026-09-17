// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_OUTPUT_HPP
#define RGBDS_LINK_OUTPUT_HPP

struct Section;

// Registers a section for output.
void out_AddSection(Section const &section);

// Finds an assigned section overlapping another one.
Section const *out_OverlappingSection(Section const &section);

void out_WriteROMIfAny();
void out_WriteSymIfAny();
void out_WriteMapIfAny();

#endif // RGBDS_LINK_OUTPUT_HPP
