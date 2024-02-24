/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <new>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "asm/macro.hpp"
#include "asm/warning.hpp"

#define MAXMACROARGS 99999

struct MacroArgs {
	unsigned int shift;
	std::vector<char *> *args;
};

static struct MacroArgs *macroArgs = NULL;
static uint32_t uniqueID = 0;
static uint32_t maxUniqueID = 0;
// The initialization is somewhat harmful, since it is never used, but it
// guarantees the size of the buffer will be correct. I was unable to find a
// better solution, but if you have one, please feel free!
static char uniqueIDBuf[] = "_u4294967295"; // UINT32_MAX
static char *uniqueIDPtr = NULL;

struct MacroArgs *macro_GetCurrentArgs(void)
{
	return macroArgs;
}

struct MacroArgs *macro_NewArgs(void)
{
	struct MacroArgs *args = (struct MacroArgs *)malloc(sizeof(*args));

	if (args)
		args->args = new(std::nothrow) std::vector<char *>();
	if (!args || !args->args)
		fatalerror("Unable to register macro arguments: %s\n", strerror(errno));

	args->shift = 0;
	return args;
}

void macro_AppendArg(struct MacroArgs *args, char *s)
{
	if (s[0] == '\0')
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument\n");
	if (args->args->size() == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS) " arguments is allowed\n");
	args->args->push_back(s);
}

void macro_UseNewArgs(struct MacroArgs *args)
{
	macroArgs = args;
}

void macro_FreeArgs(struct MacroArgs *args)
{
	for (char *arg : *macroArgs->args)
		free(arg);
	delete args->args;
}

char const *macro_GetArg(uint32_t i)
{
	if (!macroArgs)
		return NULL;

	uint32_t realIndex = i + macroArgs->shift - 1;

	return realIndex >= macroArgs->args->size() ? NULL : (*macroArgs->args)[realIndex];
}

char const *macro_GetAllArgs(void)
{
	if (!macroArgs)
		return NULL;

	size_t nbArgs = macroArgs->args->size();

	if (macroArgs->shift >= nbArgs)
		return "";

	size_t len = 0;

	for (uint32_t i = macroArgs->shift; i < nbArgs; i++)
		len += strlen((*macroArgs->args)[i]) + 1; // 1 for comma

	char *str = (char *)malloc(len + 1); // 1 for '\0'
	char *ptr = str;

	if (!str)
		fatalerror("Failed to allocate memory for expanding '\\#': %s\n", strerror(errno));

	for (uint32_t i = macroArgs->shift; i < nbArgs; i++) {
		char *arg = (*macroArgs->args)[i];
		size_t n = strlen(arg);

		memcpy(ptr, arg, n);
		ptr += n;

		// Commas go between args and after a last empty arg
		if (i < nbArgs - 1 || n == 0)
			*ptr++ = ','; // no space after comma
	}
	*ptr = '\0';

	return str;
}

uint32_t macro_GetUniqueID(void)
{
	return uniqueID;
}

char const *macro_GetUniqueIDStr(void)
{
	// Generate a new unique ID on the first use of `\@`
	if (uniqueID == 0)
		macro_SetUniqueID(++maxUniqueID);

	return uniqueIDPtr;
}

void macro_SetUniqueID(uint32_t id)
{
	uniqueID = id;
	if (id == 0 || id == (uint32_t)-1) {
		uniqueIDPtr = NULL;
	} else {
		// The buffer is guaranteed to be the correct size
		// This is a valid label fragment, but not a valid numeric
		sprintf(uniqueIDBuf, "_u%" PRIu32, id);
		uniqueIDPtr = uniqueIDBuf;
	}
}

uint32_t macro_UseNewUniqueID(void)
{
	// A new ID will be generated on the first use of `\@`
	macro_SetUniqueID(0);
	return uniqueID;
}

uint32_t macro_UndefUniqueID(void)
{
	// No ID will be generated; use of `\@` is an error
	macro_SetUniqueID((uint32_t)-1);
	return uniqueID;
}

void macro_ShiftCurrentArgs(int32_t count)
{
	if (!macroArgs) {
		error("Cannot shift macro arguments outside of a macro\n");
	} else if (size_t nbArgs = macroArgs->args->size();
		   count > 0 && ((uint32_t)count > nbArgs || macroArgs->shift > nbArgs - count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their end\n");
		macroArgs->shift = nbArgs;
	} else if (count < 0 && macroArgs->shift < (uint32_t)-count) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their beginning\n");
		macroArgs->shift = 0;
	} else {
		macroArgs->shift += count;
	}
}

uint32_t macro_NbArgs(void)
{
	return macroArgs->args->size() - macroArgs->shift;
}
