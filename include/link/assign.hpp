/* SPDX-License-Identifier: MIT */

// Assigning all sections a place
#ifndef RGBDS_LINK_ASSIGN_H
#define RGBDS_LINK_ASSIGN_H

#include <stdint.h>

extern uint64_t nbSectionsToAssign;

// Assigns all sections a slice of the address space
void assign_AssignSections();

#endif // RGBDS_LINK_ASSIGN_H
