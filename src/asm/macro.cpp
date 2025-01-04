/* SPDX-License-Identifier: MIT */

#include "asm/macro.hpp"

#include <stdio.h>
#include <string.h>
#include <string>

#include "helpers.hpp"

#include "asm/warning.hpp"

#define MAXMACROARGS 99999

std::shared_ptr<std::string> MacroArgs::getArg(uint32_t i) {
	uint32_t realIndex = i + shift - 1;

	if (realIndex >= args.size())
		return nullptr;

	auto &arg = args[realIndex];
	arg.second = true;
	return arg.first;
}

std::shared_ptr<std::string> MacroArgs::getAllArgs() {
	size_t nbArgs = args.size();

	if (shift >= nbArgs)
		return std::make_shared<std::string>("");

	size_t len = 0;

	for (uint32_t i = shift; i < nbArgs; i++)
		len += args[i].first->length() + 1; // 1 for comma

	auto str = std::make_shared<std::string>();
	str->reserve(len + 1); // 1 for comma

	for (uint32_t i = shift; i < nbArgs; i++) {
		auto &arg = args[i];

		arg.second = true;
		str->append(*arg.first);

		// Commas go between args and after a last empty arg
		if (i < nbArgs - 1 || arg.first->empty())
			str->push_back(','); // no space after comma
	}

	return str;
}

void MacroArgs::appendArg(std::shared_ptr<std::string> arg) {
	if (arg->empty())
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument\n");
	if (args.size() == MAXMACROARGS)
		error("A maximum of " EXPAND_AND_STR(MAXMACROARGS) " arguments is allowed\n");
	args.emplace_back(arg, false);
}

void MacroArgs::shiftArgs(int32_t count) {
	if (size_t nbArgs = args.size();
	    count > 0 && (static_cast<uint32_t>(count) > nbArgs || shift > nbArgs - count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their end\n");
		shift = nbArgs;
	} else if (count < 0 && shift < static_cast<uint32_t>(-count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their beginning\n");
		shift = 0;
	} else {
		shift += count;
	}
}

void MacroArgs::checkUsedArgs() const {
	for (size_t i = 0; i < args.size(); i++) {
		auto const &arg = args[i];
		if (!arg.second) {
			warning(
			    WARNING_UNUSED_MACRO_ARG,
			    i < 9 ? "Unused macro arg \\%zu\n" : "Unused macro arg \\<%zu>\n",
			    i + 1
			);
		}
	}
}
