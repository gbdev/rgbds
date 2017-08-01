#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "link/main.h"
#include "link/patch.h"
#include "types.h"

#define HASHSIZE 73

struct ISymbol {
	char *pzName;
	SLONG nValue;
	SLONG nBank; /* -1 = constant */
	char tzObjFileName[_MAX_PATH + 1]; /* Object file where the symbol was defined. */
	char tzFileName[_MAX_PATH + 1]; /* Source file where the symbol was defined. */
	ULONG nFileLine; /* Line where the symbol was defined. */
	struct ISymbol *pNext;
};

struct ISymbol *tHash[HASHSIZE];

SLONG
calchash(char *s)
{
	SLONG r = 0;
	while (*s)
		r += *s++;

	return (r % HASHSIZE);
}

void
sym_Init(void)
{
	SLONG i;
	for (i = 0; i < HASHSIZE; i += 1)
		tHash[i] = NULL;
}

SLONG
sym_GetValue(char *tzName)
{
	if (strcmp(tzName, "@") == 0) {
		return (nPC);
	} else {
		struct ISymbol **ppSym;

		ppSym = &(tHash[calchash(tzName)]);
		while (*ppSym) {
			if (strcmp(tzName, (*ppSym)->pzName)) {
				ppSym = &((*ppSym)->pNext);
			} else {
				return ((*ppSym)->nValue);
			}
		}

		errx(1, "Unknown symbol '%s'", tzName);
	}
}

SLONG
sym_GetBank(char *tzName)
{
	struct ISymbol **ppSym;

	ppSym = &(tHash[calchash(tzName)]);
	while (*ppSym) {
		if (strcmp(tzName, (*ppSym)->pzName)) {
			ppSym = &((*ppSym)->pNext);
		} else {
			return ((*ppSym)->nBank);
		}
	}

	errx(1, "Unknown symbol '%s'", tzName);
}

void
sym_CreateSymbol(char *tzName, SLONG nValue, SLONG nBank, char *tzObjFileName,
		char *tzFileName, ULONG nFileLine)
{
	if (strcmp(tzName, "@") == 0)
		return;

	struct ISymbol **ppSym;

	ppSym = &(tHash[calchash(tzName)]);

	while (*ppSym) {
		if (strcmp(tzName, (*ppSym)->pzName)) {
			ppSym = &((*ppSym)->pNext);
		} else {
			if (nBank == -1)
				return;

			errx(1, "'%s' in both %s : %s(%d) and %s : %s(%d)",
				tzName, tzObjFileName, tzFileName, nFileLine,
				(*ppSym)->tzObjFileName,
				(*ppSym)->tzFileName, (*ppSym)->nFileLine);
		}
	}

	if ((*ppSym = malloc(sizeof **ppSym))) {
		if (((*ppSym)->pzName = malloc(strlen(tzName) + 1))) {
			strcpy((*ppSym)->pzName, tzName);
			(*ppSym)->nValue = nValue;
			(*ppSym)->nBank = nBank;
			(*ppSym)->pNext = NULL;
			strncpy((*ppSym)->tzObjFileName, tzObjFileName,
				sizeof((*ppSym)->tzObjFileName));
			strncpy((*ppSym)->tzFileName, tzFileName,
				sizeof((*ppSym)->tzFileName));
			(*ppSym)->nFileLine = nFileLine;
		}
	}
}
