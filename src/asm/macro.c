
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/macro.h"
#include "asm/warning.h"

static char *currentmacroargs[MAXMACROARGS + 1];
static char *newmacroargs[MAXMACROARGS + 1];

void macro_AddNewArg(char const *s)
{
	int32_t i = 0;

	while (i < MAXMACROARGS && newmacroargs[i] != NULL)
		i++;

	if (i < MAXMACROARGS) {
		if (s)
			newmacroargs[i] = strdup(s);
		else
			newmacroargs[i] = NULL;
	} else {
		yyerror("A maximum of %d arguments allowed", MAXMACROARGS);
	}
}

void macro_SaveCurrentArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		save[i] = currentmacroargs[i];
		currentmacroargs[i] = NULL;
	}
}

void macro_RestoreCurrentArgs(char *save[])
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		free(currentmacroargs[i]);
		currentmacroargs[i] = save[i];
	}
}

void macro_UseNewArgs(void)
{
	int32_t i;

	for (i = 0; i <= MAXMACROARGS; i++) {
		free(currentmacroargs[i]);
		currentmacroargs[i] = newmacroargs[i];
		newmacroargs[i] = NULL;
	}
}

char *macro_FindArg(int32_t i)
{
	if (i == -1)
		i = MAXMACROARGS + 1;

	assert(i >= 1);

	assert((size_t)(i - 1)
	       < sizeof(currentmacroargs) / sizeof(*currentmacroargs));

	return currentmacroargs[i - 1];
}

void macro_UseCurrentArgs(void)
{
	int32_t i;

	for (i = 1; i <= MAXMACROARGS; i++)
		macro_AddNewArg(macro_FindArg(i));
}

void macro_SetArgID(uint32_t nMacroCount)
{
	char s[256];

	snprintf(s, sizeof(s) - 1, "_%u", nMacroCount);
	newmacroargs[MAXMACROARGS] = strdup(s);
}

void macro_ShiftCurrentArgs(void)
{
	int32_t i;

	free(currentmacroargs[0]);
	for (i = 0; i < MAXMACROARGS - 1; i++)
		currentmacroargs[i] = currentmacroargs[i + 1];

	currentmacroargs[MAXMACROARGS - 1] = NULL;
}

uint32_t macro_NbArgs(void)
{
	uint32_t i = 0;

	while (currentmacroargs[i] && i < MAXMACROARGS)
		i++;

	return i;
}

void macro_Init(void)
{
	for (uint32_t i = 0; i < MAXMACROARGS; i++) {
		currentmacroargs[i] = NULL;
		newmacroargs[i] = NULL;
	}
}
