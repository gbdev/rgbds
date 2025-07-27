// SPDX-License-Identifier: MIT

#ifndef RGBDS_USAGE_HPP
#define RGBDS_USAGE_HPP

#include <stdarg.h>

class Usage {
	char const *usage;

public:
	Usage(char const *usage_) : usage(usage_) {}

	[[noreturn]]
	void printAndExit(int code) const;

	[[gnu::format(printf, 2, 3), noreturn]]
	void printAndExit(char const *fmt, ...) const;
};

#endif // RGBDS_USAGE_HPP
