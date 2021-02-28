/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2021, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Fixed-point math routines
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "asm/fixpoint.h"
#include "asm/symbol.h"
#include "asm/warning.h"

#define fix2double(i)	((double)((i) / 65536.0))
#define double2fix(d)	((int32_t)round((d) * 65536.0))

// pi radians == 32768 fixed-point "degrees"
#define fdeg2rad(f)	((f) * (M_PI / 32768.0))
#define rad2fdeg(r)	((r) * (32768.0 / M_PI))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Return the _PI symbol value
 */
int32_t fix_Callback_PI(void)
{
	warning(WARNING_OBSOLETE, "`_PI` is deprecated; use 3.14159\n");

	return double2fix(M_PI);
}

/*
 * Print a fixed point value
 */
void fix_Print(int32_t i)
{
	uint32_t u = i;
	const char *sign = "";

	if (i < 0) {
		u = -u;
		sign = "-";
	}

	printf("%s%" PRIu32 ".%05" PRIu32, sign, u >> 16,
	       ((uint32_t)(fix2double(u) * 100000 + 0.5)) % 100000);
}

/*
 * Calculate sine
 */
int32_t fix_Sin(int32_t i)
{
	return double2fix(sin(fdeg2rad(fix2double(i))));
}

/*
 * Calculate cosine
 */
int32_t fix_Cos(int32_t i)
{
	return double2fix(cos(fdeg2rad(fix2double(i))));
}

/*
 * Calculate tangent
 */
int32_t fix_Tan(int32_t i)
{
	return double2fix(tan(fdeg2rad(fix2double(i))));
}

/*
 * Calculate arcsine
 */
int32_t fix_ASin(int32_t i)
{
	return double2fix(rad2fdeg(asin(fix2double(i))));
}

/*
 * Calculate arccosine
 */
int32_t fix_ACos(int32_t i)
{
	return double2fix(rad2fdeg(acos(fix2double(i))));
}

/*
 * Calculate arctangent
 */
int32_t fix_ATan(int32_t i)
{
	return double2fix(rad2fdeg(atan(fix2double(i))));
}

/*
 * Calculate atan2
 */
int32_t fix_ATan2(int32_t i, int32_t j)
{
	return double2fix(rad2fdeg(atan2(fix2double(i), fix2double(j))));
}

/*
 * Multiplication
 */
int32_t fix_Mul(int32_t i, int32_t j)
{
	return double2fix(fix2double(i) * fix2double(j));
}

/*
 * Division
 */
int32_t fix_Div(int32_t i, int32_t j)
{
	return double2fix(fix2double(i) / fix2double(j));
}

/*
 * Power
 */
int32_t fix_Pow(int32_t i, int32_t j)
{
	return double2fix(pow(fix2double(i), fix2double(j)));
}

/*
 * Logarithm
 */
int32_t fix_Log(int32_t i, int32_t j)
{
	return double2fix(log(fix2double(i)) / log(fix2double(j)));
}

/*
 * Round
 */
int32_t fix_Round(int32_t i)
{
	return double2fix(round(fix2double(i)));
}

/*
 * Ceil
 */
int32_t fix_Ceil(int32_t i)
{
	return double2fix(ceil(fix2double(i)));
}

/*
 * Floor
 */
int32_t fix_Floor(int32_t i)
{
	return double2fix(floor(fix2double(i)));
}
