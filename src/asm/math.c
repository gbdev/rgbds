/*
 * Fixedpoint math routines
 */

#include <math.h>
#include <stdio.h>

#include "types.h"
#include "asm/mymath.h"
#include "asm/symbol.h"

#define fix2double(i)	((double)(i/65536.0))
#define double2fix(d)	((SLONG)(d*65536.0))
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
math_Print(SLONG i)
{
	if (i >= 0)
		printf("%ld.%05ld", i >> 16,
		    ((SLONG) (fix2double(i) * 100000 + 0.5)) % 100000);
	else
		printf("-%ld.%05ld", (-i) >> 16,
		    ((SLONG) (fix2double(-i) * 100000 + 0.5)) % 100000);
}

/*
 * Calculate sine
 */
SLONG
math_Sin(SLONG i)
{
	return (double2fix(sin(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate cosine
 */
SLONG
math_Cos(SLONG i)
{
	return (double2fix(cos(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate tangent
 */
SLONG
math_Tan(SLONG i)
{
	return (double2fix(tan(fix2double(i) * 2 * PI / 65536)));
}

/*
 * Calculate arcsine
 */
SLONG
math_ASin(SLONG i)
{
	return (double2fix(asin(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate arccosine
 */
SLONG
math_ACos(SLONG i)
{
	return (double2fix(acos(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate arctangent
 */
SLONG
math_ATan(SLONG i)
{
	return (double2fix(atan(fix2double(i)) / 2 / PI * 65536));
}

/*
 * Calculate atan2
 */
SLONG
math_ATan2(SLONG i, SLONG j)
{
	return (double2fix
	    (atan2(fix2double(i), fix2double(j)) / 2 / PI * 65536));
}

/*
 * Multiplication
 */
SLONG
math_Mul(SLONG i, SLONG j)
{
	return (double2fix(fix2double(i) * fix2double(j)));
}

/*
 * Division
 */
SLONG
math_Div(SLONG i, SLONG j)
{
	return (double2fix(fix2double(i) / fix2double(j)));
}

/*
 * Round
 */
SLONG
math_Round(SLONG i)
{
	return double2fix(round(fix2double(i)));
}

/*
 * Ceil
 */
SLONG
math_Ceil(SLONG i)
{
	return double2fix(ceil(fix2double(i)));
}

/*
 * Floor
 */
SLONG
math_Floor(SLONG i)
{
	return double2fix(floor(fix2double(i)));
}
