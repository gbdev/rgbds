/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_ASSIGN_HPP
#define RGBDS_LINK_ASSIGN_HPP

#include <stdint.h>

extern uint64_t nbSectionsToAssign;

// Assigns all sections a slice of the address space
void assign_AssignSections();

#endif // RGBDS_LINK_ASSIGN_HPP
