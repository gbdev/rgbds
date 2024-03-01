/* SPDX-License-Identifier: MIT */

// Outputting the result of linking
#ifndef RGBDS_LINK_OUTPUT_H
#define RGBDS_LINK_OUTPUT_H

#include <stdint.h>

#include "link/section.hpp"

/*
 * Registers a section for output.
 * @param section The section to add
 */
void out_AddSection(Section const *section);

/*
 * Finds an assigned section overlapping another one.
 * @param section The section that is being overlapped
 * @return A section overlapping it
 */
Section const *out_OverlappingSection(Section const *section);

/*
 * Writes all output (bin, sym, map) files.
 */
void out_WriteFiles();

#endif // RGBDS_LINK_OUTPUT_H
