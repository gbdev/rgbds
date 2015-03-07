#ifndef RGBDS_ASM_MATH_H
#define RGBDS_ASM_MATH_H

#include "types.h"

void math_DefinePI(void);
void math_Print(SLONG i);
SLONG math_Sin(SLONG i);
SLONG math_Cos(SLONG i);
SLONG math_Tan(SLONG i);
SLONG math_ASin(SLONG i);
SLONG math_ACos(SLONG i);
SLONG math_ATan(SLONG i);
SLONG math_ATan2(SLONG i, SLONG j);
SLONG math_Mul(SLONG i, SLONG j);
SLONG math_Div(SLONG i, SLONG j);
SLONG math_Round(SLONG i);
SLONG math_Ceil(SLONG i);
SLONG math_Floor(SLONG i);

#endif
