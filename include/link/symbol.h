#ifndef RGBDS_LINK_SYMBOL_H
#define RGBDS_LINK_SYMBOL_H

#include "types.h"

void sym_Init(void);
void sym_CreateSymbol(char *tzName, SLONG nValue, SLONG nBank,
			char *tzObjFileName, char *tzFileName, ULONG nFileLine);
SLONG sym_GetValue(char *tzName);
SLONG sym_GetBank(char *tzName);

#endif
