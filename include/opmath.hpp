// SPDX-License-Identifier: MIT

#ifndef RGBDS_OP_MATH_HPP
#define RGBDS_OP_MATH_HPP

#include <stdint.h>

int32_t op_divide(int32_t dividend, int32_t divisor);
int32_t op_modulo(int32_t dividend, int32_t divisor);
int32_t op_exponent(int32_t base, uint32_t power);

int32_t op_shift_left(int32_t value, int32_t amount);
int32_t op_shift_right(int32_t value, int32_t amount);
int32_t op_shift_right_unsigned(int32_t value, int32_t amount);

int32_t op_neg(int32_t value);

int32_t op_high(int32_t value);
int32_t op_low(int32_t value);

int32_t op_bitwidth(int32_t value);
int32_t op_tzcount(int32_t value);

#endif // RGBDS_OP_MATH_HPP
