// SPDX-License-Identifier: MIT

#ifndef RGBDS_UTIL_HPP
#define RGBDS_UTIL_HPP

#include <stddef.h>
#include <string>
#include <unordered_map>

#include "helpers.hpp"

bool startsIdentifier(int c);
bool continuesIdentifier(int c);

char const *printChar(int c);

struct Uppercase {
	size_t operator()(std::string const &str) const;
	bool operator()(std::string const &str1, std::string const &str2) const;
};

template<typename T>
using UpperMap = std::unordered_map<std::string, T, Uppercase, Uppercase>;

#endif // RGBDS_UTIL_HPP
