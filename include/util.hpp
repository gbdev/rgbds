// SPDX-License-Identifier: MIT

#ifndef RGBDS_UTIL_HPP
#define RGBDS_UTIL_HPP

#include <algorithm>
#include <ctype.h>
#include <numeric>
#include <stddef.h>
#include <string>
#include <unordered_map>

#include "helpers.hpp"

bool isNewline(int c);
bool isBlankSpace(int c);
bool isWhitespace(int c);
bool isPrintable(int c);

bool startsIdentifier(int c);
bool continuesIdentifier(int c);

char const *printChar(int c);

struct Uppercase {
	// FNV-1a hash of an uppercased string
	constexpr size_t operator()(std::string const &str) const {
		return std::accumulate(RANGE(str), 0x811C9DC5, [](size_t hash, char c) {
			return (hash ^ toupper(c)) * 16777619;
		});
	}

	// Compare two strings without case-sensitivity (by converting to uppercase)
	constexpr bool operator()(std::string const &str1, std::string const &str2) const {
		return std::equal(RANGE(str1), RANGE(str2), [](char c1, char c2) {
			return toupper(c1) == toupper(c2);
		});
	}
};

template<typename T>
using UpperMap = std::unordered_map<std::string, T, Uppercase, Uppercase>;

#endif // RGBDS_UTIL_HPP
