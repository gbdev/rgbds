/* SPDX-License-Identifier: MIT */

#include "asm/macro.hpp"

#include <errno.h>
#include <inttypes.h>
#include <new>
#include <stdio.h>
#include <string.h>

#define MAXMACROARGS 99999

static MacroArgs *macroArgs = nullptr;

void MacroArgs::append(std::string s) {
	if (s.empty())
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument\n");
	if (args.size() == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS) " arguments is allowed\n");
	args.push_back(s);
}

MacroArgs *macro_GetCurrentArgs() {
	return macroArgs;
}

void macro_UseNewArgs(MacroArgs *args) {
	macroArgs = args;
}

char const *macro_GetArg(uint32_t i) {
	if (!macroArgs)
		return nullptr;

	uint32_t realIndex = i + macroArgs->shift - 1;

	return realIndex >= macroArgs->args.size() ? nullptr : macroArgs->args[realIndex].c_str();
}

char const *macro_GetAllArgs() {
	if (!macroArgs)
		return nullptr;

	size_t nbArgs = macroArgs->args.size();

	if (macroArgs->shift >= nbArgs)
		return "";

	size_t len = 0;

	for (uint32_t i = macroArgs->shift; i < nbArgs; i++)
		len += macroArgs->args[i].length() + 1; // 1 for comma

	char *str = new (std::nothrow) char[len + 1]; // 1 for '\0'
	char *ptr = str;

	if (!str)
		fatalerror("Failed to allocate memory for expanding '\\#': %s\n", strerror(errno));

	for (uint32_t i = macroArgs->shift; i < nbArgs; i++) {
		std::string const &arg = macroArgs->args[i];
		size_t n = arg.length();

		memcpy(ptr, arg.c_str(), n);
		ptr += n;

		// Commas go between args and after a last empty arg
		if (i < nbArgs - 1 || n == 0)
			*ptr++ = ','; // no space after comma
	}
	*ptr = '\0';

	return str;
}

void macro_ShiftCurrentArgs(int32_t count) {
	if (!macroArgs) {
		error("Cannot shift macro arguments outside of a macro\n");
	} else if (size_t nbArgs = macroArgs->args.size();
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

uint32_t macro_NbArgs() {
	return macroArgs->args.size() - macroArgs->shift;
}
