
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/macro.h"
#include "asm/warning.h"

/*
 * Your average macro invocation does not go past the tens, but some go further
 * This ensures that sane and slightly insane invocations suffer no penalties,
 * and the rest is insane and thus will assume responsibility.
 * Additionally, ~300 bytes (on x64) of memory per level of nesting has been
 * deemed reasonable. (Halve that on x86.)
 */
#define INITIAL_ARG_SIZE 32
struct MacroArgs {
	unsigned int nbArgs;
	unsigned int shift;
	unsigned int capacity;
	char *args[];
};

#define SIZEOF_ARGS(nbArgs) (sizeof(struct MacroArgs) + \
			    sizeof(((struct MacroArgs){0}).args[0]) * (nbArgs))

static struct MacroArgs *macroArgs = NULL;
static uint32_t uniqueID = 0;
static uint32_t maxUniqueID = 0;
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
	struct MacroArgs *args = malloc(SIZEOF_ARGS(INITIAL_ARG_SIZE));

	if (!args)
		fatalerror("Unable to register macro arguments: %s\n", strerror(errno));

	args->nbArgs = 0;
	args->shift = 0;
	args->capacity = INITIAL_ARG_SIZE;
	return args;
}

void macro_AppendArg(struct MacroArgs **argPtr, char *s)
{
#define macArgs (*argPtr)
	if (macArgs->nbArgs == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS)
		      " arguments is allowed\n");
	if (macArgs->nbArgs >= macArgs->capacity) {
		macArgs->capacity *= 2;
		/* Check that overflow didn't roll us back */
		if (macArgs->capacity <= macArgs->nbArgs)
			fatalerror("Failed to add new macro argument: possible capacity overflow\n");
		macArgs = realloc(macArgs, SIZEOF_ARGS(macArgs->capacity));
		if (!macArgs)
			fatalerror("Error adding new macro argument: %s\n", strerror(errno));
	}
	macArgs->args[macArgs->nbArgs++] = s;
#undef macArgs
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
	if (!macroArgs)
		return NULL;

	uint32_t realIndex = i + macroArgs->shift - 1;

	return realIndex >= macroArgs->nbArgs ? NULL
					      : macroArgs->args[realIndex];
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
	if (id == 0) {
		uniqueIDPtr = NULL;
	} else {
		if (uniqueID > maxUniqueID)
			maxUniqueID = uniqueID;
		/* The buffer is guaranteed to be the correct size */
		sprintf(uniqueIDBuf, "_%" PRIu32, id);
		uniqueIDPtr = uniqueIDBuf;
	}
}

uint32_t macro_UseNewUniqueID(void)
{
	macro_SetUniqueID(++maxUniqueID);
	return maxUniqueID;
}

void macro_ShiftCurrentArgs(int32_t count)
{
	if (!macroArgs) {
		error("Cannot shift macro arguments outside of a macro\n");
	} else if (count < 0) {
		error("Cannot shift arguments by negative amount %" PRId32 "\n", count);
	} else if (macroArgs->shift < macroArgs->nbArgs) {
		macroArgs->shift += count;
		if (macroArgs->shift > macroArgs->nbArgs)
			macroArgs->shift = macroArgs->nbArgs;
	}
}

uint32_t macro_NbArgs(void)
{
	return macroArgs->nbArgs - macroArgs->shift;
}
