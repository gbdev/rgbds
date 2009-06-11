#ifndef ASMOTOR_LIB_LIBWRAP_H
#define ASMOTOR_LIB_LIBWRAP_H

#include "lib/types.h"

#define MAXNAMELENGTH	256

struct LibraryWrapper {
	char tName[MAXNAMELENGTH];
	UWORD uwTime;
	UWORD uwDate;
	SLONG nByteLength;
	UBYTE *pData;
	struct LibraryWrapper *pNext;
};

typedef struct LibraryWrapper sLibrary;

#endif
