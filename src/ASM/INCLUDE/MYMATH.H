#ifndef	MATH_H
#define	MATH_H

#include "types.h"

void math_DefinePI( void );
void math_Print( SLONG i );
SLONG math_Sin( SLONG i );
SLONG math_Cos( SLONG i );
SLONG math_Tan( SLONG i );
SLONG math_ASin( SLONG i );
SLONG math_ACos( SLONG i );
SLONG math_ATan( SLONG i );
SLONG math_ATan2( SLONG i, SLONG j );
SLONG math_Mul( SLONG i, SLONG j );
SLONG math_Div( SLONG i, SLONG j );

#endif
