/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_SDAS_OBJ_HPP
#define RGBDS_LINK_SDAS_OBJ_HPP

#include <stdio.h>
#include <vector>

struct FileStackNode;
struct Symbol;

void sdobj_ReadFile(FileStackNode const &where, FILE *file, std::vector<Symbol> &fileSymbols);

#endif // RGBDS_LINK_SDAS_OBJ_HPP
