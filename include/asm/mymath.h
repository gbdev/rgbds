/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_MATH_H
#define RGBDS_ASM_MATH_H

#include <stdint.h>

void math_DefinePI(void);
void math_Print(int32_t i);
int32_t math_Sin(int32_t i);
int32_t math_Cos(int32_t i);
int32_t math_Tan(int32_t i);
int32_t math_ASin(int32_t i);
int32_t math_ACos(int32_t i);
int32_t math_ATan(int32_t i);
int32_t math_ATan2(int32_t i, int32_t j);
int32_t math_Mul(int32_t i, int32_t j);
int32_t math_Div(int32_t i, int32_t j);
int32_t math_Round(int32_t i);
int32_t math_Ceil(int32_t i);
int32_t math_Floor(int32_t i);

#endif /* RGBDS_ASM_MATH_H */
