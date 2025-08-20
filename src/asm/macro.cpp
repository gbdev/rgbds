// SPDX-License-Identifier: MIT

#include "asm/macro.hpp"

#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "asm/warning.hpp"

std::shared_ptr<std::string> MacroArgs::getArg(int32_t i) const {
	// Bracketed macro arguments adjust negative indexes such that -1 is the last argument.
	if (i < 0) {
		i += args.size() + 1;
	}

	int32_t realIndex = i + shift - 1;

	return realIndex < 0 || static_cast<uint32_t>(realIndex) >= args.size() ? nullptr
	                                                                        : args[realIndex];
}

std::shared_ptr<std::string> MacroArgs::getAllArgs() const {
	size_t nbArgs = args.size();

	if (shift >= nbArgs) {
		return std::make_shared<std::string>("");
	}

	size_t len = 0;

	for (uint32_t i = shift; i < nbArgs; ++i) {
		len += args[i]->length() + 1; // 1 for comma
	}

	auto str = std::make_shared<std::string>();
	str->reserve(len + 1); // 1 for comma

	for (uint32_t i = shift; i < nbArgs; ++i) {
		std::shared_ptr<std::string> const &arg = args[i];

		str->append(*arg);

		// Commas go between args and after a last empty arg
		if (i < nbArgs - 1 || arg->empty()) {
			str->push_back(','); // no space after comma
		}
	}

	return str;
}

void MacroArgs::appendArg(std::shared_ptr<std::string> arg) {
	if (arg->empty()) {
		warning(WARNING_EMPTY_MACRO_ARG, "Empty macro argument");
	}
	args.push_back(arg);
}

void MacroArgs::shiftArgs(int32_t count) {
	if (size_t nbArgs = args.size();
	    count > 0 && (static_cast<uint32_t>(count) > nbArgs || shift > nbArgs - count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their end");
		shift = nbArgs;
	} else if (count < 0 && shift < static_cast<uint32_t>(-count)) {
		warning(WARNING_MACRO_SHIFT, "Cannot shift macro arguments past their beginning");
		shift = 0;
	} else {
		shift += count;
	}
}
