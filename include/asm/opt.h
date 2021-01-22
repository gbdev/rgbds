/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2021, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_OPT_H
#define RGBDS_OPT_H


void opt_B(char chars[2]);
void opt_G(char chars[4]);
void opt_P(uint8_t fill);
void opt_Parse(char const *option);

void opt_Push(void);
void opt_Pop(void);


#endif
