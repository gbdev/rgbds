/* SPDX-License-Identifier: MIT */

#include "asm/macro.hpp"

#include <stdio.h>
#include <string.h>
#include <string>

#include "helpers.hpp"

#include "asm/warning.hpp"

#define MAXMACROARGS 99999

std::shared_ptr<std::string> MacroArgs::getArg(uint32_t i) const {
	uint32_t realIndex = i + shift - 1;

	return realIndex >= args.size() ? nullptr : args[realIndex];
}

std::shared_ptr<std::string> MacroArgs::getAllArgs() const {
	size_t nbArgs = args.size();

	if (shift >= nbArgs)
		return std::make_shared<std::string>("");

	size_t len = 0;

	for (uint32_t i = shift; i < nbArgs; i++)
		len += args[i]->length() + 1; // 1 for comma

	auto str = std::make_shared<std::string>();
	str->reserve(len + 1); // 1 for comma

	for (uint32_t i = shift; i < nbArgs; i++) {
		auto const &arg = args[i];

		str->append(*arg);

		// Commas go between args and after a last empty arg
		if (i < nbArgs - 1 || arg->empty())
			str->push_back(','); // no space after comma
	}

	return str;
}

void MacroArgs::appendArg(std::shared_ptr<std::string> arg) {
	if (arg->empty())
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument\n");
	if (args.size() == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS) " arguments is allowed\n");
	args.push_back(arg);
}

void MacroArgs::shiftArgs(int32_t count) {
	if (size_t nbArgs = args.size();
	    count > 0 && ((uint32_t)count > nbArgs || shift > nbArgs - count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their end\n");
		shift = nbArgs;
	} else if (count < 0 && shift < (uint32_t)-count) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their beginning\n");
		shift = 0;
	} else {
		shift += count;
	}
}
