#ifndef ASMOTOR_LINK_SYMBOL_H
#define ASMOTOR_LINK_SYMBOL_H

#include "link/types.h"

void sym_Init(void);
void sym_CreateSymbol(char *tzName, SLONG nValue, SBYTE nBank);
SLONG sym_GetValue(char *tzName);
SLONG sym_GetBank(char *tzName);

#endif
