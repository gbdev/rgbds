/* SPDX-License-Identifier: MIT */

// Assigning all sections a place
#ifndef RGBDS_LINK_SDAS_OBJ_H
#define RGBDS_LINK_SDAS_OBJ_H

#include <stdio.h>
#include <vector>

struct FileStackNode;
struct Symbol;

void sdobj_ReadFile(FileStackNode const &where, FILE *file, std::vector<Symbol> &fileSymbols);

#endif // RGBDS_LINK_SDAS_OBJ_H
