/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINK_SYMBOL_H
#define RGBDS_LINK_SYMBOL_H

#include <stdint.h>

void sym_Init(void);
void sym_CreateSymbol(char *tzName, int32_t nValue, int32_t nBank,
		      char *tzObjFileName, char *tzFileName,
		      uint32_t nFileLine);
int32_t sym_GetValue(char *tzName);
int32_t sym_GetBank(char *tzName);

#endif /* RGBDS_LINK_SYMBOL_H */
