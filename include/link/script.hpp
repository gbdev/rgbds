/* SPDX-License-Identifier: MIT */

// Parsing a linker script
#ifndef RGBDS_LINK_SCRIPT_H
#define RGBDS_LINK_SCRIPT_H

#include <stdint.h>

#include "linkdefs.hpp"

/*
 * Parses the linker script, and modifies the sections mentioned within appropriately.
 */
void script_ProcessScript(char const *path);

#endif // RGBDS_LINK_SCRIPT_H
