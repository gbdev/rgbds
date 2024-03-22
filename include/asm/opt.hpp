/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_OPT_H
#define RGBDS_OPT_H

#include <stdint.h>

void opt_B(char const chars[2]);
void opt_G(char const chars[4]);
void opt_P(uint8_t padByte);
void opt_Q(uint8_t precision);
void opt_W(char const *flag);
void opt_Parse(char const *option);

void opt_Push();
void opt_Pop();

#endif // RGBDS_OPT_H
