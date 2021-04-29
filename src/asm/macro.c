
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/macro.h"
#include "asm/string.h"
#include "asm/warning.h"

#define MAXMACROARGS 99999

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
	struct String *args[];
};

#define SIZEOF_ARGS(nbArgs) (sizeof(struct MacroArgs) + \
			    sizeof(((struct MacroArgs){0}).args[0]) * (nbArgs))

static struct MacroArgs *macroArgs = NULL;
static uint32_t uniqueID = 0;
static uint32_t maxUniqueID = 0;
static uint32_t strUniqueID = 0; // The uniqueID contained within the uniqueIDStr
static struct String *uniqueIDStr = NULL;

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

/**
 * Takes ownership of the string
 */
void macro_AppendArg(struct MacroArgs **argPtr, struct String *str)
{
#define macArgs (*argPtr)
	if (str_Index(str, 0) == '\0')
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument\n");
	if (macArgs->nbArgs == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS) " arguments is allowed\n");
	if (macArgs->nbArgs >= macArgs->capacity) {
		macArgs->capacity *= 2;
		/* Check that overflow didn't roll us back */
		if (macArgs->capacity <= macArgs->nbArgs)
			fatalerror("Failed to add new macro argument: capacity overflow\n");
		macArgs = realloc(macArgs, SIZEOF_ARGS(macArgs->capacity));
		if (!macArgs)
			fatalerror("Error adding new macro argument: %s\n", strerror(errno));
	}
	macArgs->args[macArgs->nbArgs++] = str;
#undef macArgs
}

void macro_UseNewArgs(struct MacroArgs *args)
{
	macroArgs = args;
}

void macro_FreeArgs(struct MacroArgs *args)
{
	for (uint32_t i = 0; i < macroArgs->nbArgs; i++)
		str_Unref(args->args[i]);
}

struct String *macro_GetArg(uint32_t i)
{
	if (!macroArgs)
		return NULL;

	uint32_t realIndex = i + macroArgs->shift - 1;

	return realIndex >= macroArgs->nbArgs ? NULL
					      : macroArgs->args[realIndex];
}

struct String *macro_GetAllArgs(void)
{
	if (!macroArgs)
		return NULL;

	size_t len = 0;

	for (uint32_t i = macroArgs->shift; i < macroArgs->nbArgs; i++)
		len += str_Len(macroArgs->args[i]) + 1; // 1 for comma

	struct String *str = str_New(len); // Technically that's 1 too much, but eh

	if (!str)
		fatalerror("Failed to allocate memory for expanding '\\#': %s\n", strerror(errno));

	for (uint32_t i = macroArgs->shift; i < macroArgs->nbArgs; i++) {
		str = str_Append(str, macroArgs->args[i]);

		// Commas go between args and after a last empty arg
		if (i < macroArgs->nbArgs - 1 || str_Len(macroArgs->args[i]) == 0)
			str = str_Push(str, ','); // No space after comma
	}

	return str;
}

uint32_t macro_GetUniqueID(void)
{
	return uniqueID;
}

struct String *macro_GetUniqueIDStr(void)
{
	// Unique ID hasn't changed since the last time we generated the string, return it
	if (strUniqueID == uniqueID && uniqueIDStr)
		return uniqueIDStr;

	if (!uniqueIDStr) {
		// Make room for the terminator because `sprintf` and I'm lazy
		uniqueIDStr = str_New(strlen("_u4294967295") + 1); // UINT32_MAX
		if (!uniqueIDStr)
			fatalerror("Failed to alloc unique ID str: %s\n", strerror(errno));
		char *ptr = str_CharsMut(uniqueIDStr);

		ptr[0] = '_';
		ptr[1] = 'u';
	}

	// Write the number
	int len = sprintf(&str_CharsMut(uniqueIDStr)[2], "%u", uniqueID);

	if (len < 0) {
		error("Failed to print unique ID str: %s\n", strerror(errno));
		return NULL;
	}
	str_SetLen(uniqueIDStr, len);
	strUniqueID = uniqueID; // Remember which ID the string is now storing

	return uniqueIDStr;
}

void macro_SetUniqueID(uint32_t id)
{
	uniqueID = id;
}

uint32_t macro_UseNewUniqueID(void)
{
	macro_SetUniqueID(++maxUniqueID);
	return uniqueID;
}

void macro_ShiftCurrentArgs(int32_t count)
{
	if (!macroArgs) {
		error("Cannot shift macro arguments outside of a macro\n");
	} else if (count > 0 && ((uint32_t)count > macroArgs->nbArgs
		   || macroArgs->shift > macroArgs->nbArgs - count)) {
		warning(WARNING_MACRO_SHIFT,
			"Cannot shift macro arguments past their end\n");
		macroArgs->shift = macroArgs->nbArgs;
	} else if (count < 0 && macroArgs->shift < (uint32_t)-count) {
		warning(WARNING_MACRO_SHIFT,
			"Cannot shift macro arguments past their beginning\n");
		macroArgs->shift = 0;
	} else {
		macroArgs->shift += count;
	}
}

uint32_t macro_NbArgs(void)
{
	return macroArgs->nbArgs - macroArgs->shift;
}
