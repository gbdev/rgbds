
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/macro.h"
#include "asm/warning.h"

struct MacroArgs {
	char *args[MAXMACROARGS];
	unsigned int nbArgs;
	unsigned int shift;
};

static struct MacroArgs defaultArgs = { .nbArgs = 0, .shift = 0 };
static struct MacroArgs *macroArgs = &defaultArgs;
static uint32_t uniqueID = -1;
/*
 * The initialization is somewhat harmful, since it is never used, but it
 * guarantees the size of the buffer will be correct. I was unable to find a
 * better solution, but if you have one, please feel free!
 */
static char uniqueIDBuf[] = "_" EXPAND_AND_STR(UINT32_MAX);
static char *uniqueIDPtr = NULL;

struct MacroArgs *macro_GetCurrentArgs(void)
{
	return macroArgs;
}

struct MacroArgs *macro_NewArgs(void)
{
	struct MacroArgs *args = malloc(sizeof(*args));

	args->nbArgs = 0;
	args->shift = 0;
	return args;
}

void macro_AppendArg(struct MacroArgs *args, char *s)
{
	if (args->nbArgs == MAXMACROARGS)
		yyerror("A maximum of " EXPAND_AND_STR(MAXMACROARGS)
			" arguments is allowed");
	args->args[args->nbArgs++] = s;
}

void macro_UseNewArgs(struct MacroArgs *args)
{
	macroArgs = args;
}

void macro_FreeArgs(struct MacroArgs *args)
{
	for (uint32_t i = 0; i < macroArgs->nbArgs; i++)
		free(args->args[i]);
}

char const *macro_GetArg(uint32_t i)
{
	uint32_t realIndex = i + macroArgs->shift - 1;

	return realIndex >= MAXMACROARGS ? NULL : macroArgs->args[realIndex];
}

uint32_t macro_GetUniqueID(void)
{
	return uniqueID;
}

char const *macro_GetUniqueIDStr(void)
{
	return uniqueIDPtr;
}

void macro_SetUniqueID(uint32_t id)
{
	uniqueID = id;
	if (id == -1) {
		uniqueIDPtr = NULL;
	} else {
		/* The buffer is guaranteed to be the correct size */
		sprintf(uniqueIDBuf, "_%u", id);
		uniqueIDPtr = uniqueIDBuf;
	}
}

void macro_ShiftCurrentArgs(void)
{
	if (macroArgs->shift != macroArgs->nbArgs)
		macroArgs->shift++;
}

uint32_t macro_NbArgs(void)
{
	return macroArgs->nbArgs - macroArgs->shift;
}
