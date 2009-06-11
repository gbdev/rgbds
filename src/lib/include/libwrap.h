#ifndef	LIBWRAP_H
#define	LIBWRAP_H

#include	"types.h"

#define	MAXNAMELENGTH	256

struct	LibraryWrapper
{
	char		tName[MAXNAMELENGTH];
	UWORD		uwTime;
	UWORD		uwDate;
	SLONG		nByteLength;
	UBYTE		*pData;
	struct	LibraryWrapper	*pNext;
};

typedef	struct	LibraryWrapper	sLibrary;

#endif