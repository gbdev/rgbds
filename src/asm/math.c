/*
 * Fixedpoint math routines
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "asm/mymath.h"
#include "asm/symbol.h"

#define fix2double(i)	((double)(i/65536.0))
#define double2fix(d)	((int32_t)(d*65536.0))
#ifndef PI
#define PI					(acos(-1))
#endif

/*
 * Define the _PI symbol
 */
void
math_DefinePI(void)
{
	sym_AddEqu("_PI", double2fix(PI));
}

/*
 * Print a fixed point value
 */
void
math_Print(int32_t i)
{
	if (i >= 0)
		printf("%d.%05d", i >> 16,
		    ((int32_t) (fix2double(i) * 100000 + 0.5)) % 100000);
	else
		printf("-%d.%05d", (-i) >> 16,
		    ((int32_t) (fix2double(-i) * 100000 + 0.5)) % 100000);
}

/*
 * Calculate sine
 */
int32_t
math_Sin(int32_t i)
{
	return (double2fix(sin(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate cosine
 */
int32_t
math_Cos(int32_t i)
{
	return (double2fix(cos(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate tangent
 */
int32_t
math_Tan(int32_t i)
{
	return (double2fix(tan(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate arcsine
 */
int32_t
math_ASin(int32_t i)
{
	return (double2fix(asin(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate arccosine
 */
int32_t
math_ACos(int32_t i)
{
	return (double2fix(acos(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate arctangent
 */
int32_t
math_ATan(int32_t i)
{
	return (double2fix(atan(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate atan2
 */
int32_t
math_ATan2(int32_t i, int32_t j)
{
	return (double2fix
	    (atan2(fix2double(i), fix2double(j)) / 2 / PI * 65536));
}

/*
 * Multiplication
 */
int32_t
math_Mul(int32_t i, int32_t j)
{
	return (double2fix(fix2double(i) * fix2double(j)));
}

/*
 * Division
 */
int32_t
math_Div(int32_t i, int32_t j)
{
	return (double2fix(fix2double(i) / fix2double(j)));
}

/*
 * Round
 */
int32_t
math_Round(int32_t i)
{
	return double2fix(round(fix2double(i)));
}

/*
 * Ceil
 */
int32_t
math_Ceil(int32_t i)
{
	return double2fix(ceil(fix2double(i)));
}

/*
 * Floor
 */
int32_t
math_Floor(int32_t i)
{
	return double2fix(floor(fix2double(i)));
}
