// SPDX-License-Identifier: MIT

#ifndef RGBDS_USAGE_HPP
#define RGBDS_USAGE_HPP

#include <stdarg.h>
#include <string>
#include <utility>
#include <vector>

struct Usage {
	std::string name;
	std::vector<std::string> flags;
	std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> options;

	[[noreturn]]
	void printAndExit(int code) const;

	[[gnu::format(printf, 2, 3), noreturn]]
	void printAndExit(char const *fmt, ...) const;
};

#endif // RGBDS_USAGE_HPP
