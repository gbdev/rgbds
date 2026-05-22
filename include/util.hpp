// SPDX-License-Identifier: MIT

#ifndef RGBDS_UTIL_HPP
#define RGBDS_UTIL_HPP

#include <algorithm>
#include <numeric>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include "helpers.hpp"

enum NumberBase {
	BASE_AUTO = 0,
	BASE_2 = 2,
	BASE_8 = 8,
	BASE_10 = 10,
	BASE_16 = 16,
};

// Locale-independent character class functions
bool isNewline(int c);
bool isBlankSpace(int c);
bool isWhitespace(int c);
bool isPrintable(int c);
bool isUpper(int c);
bool isLower(int c);
bool isLetter(int c);
bool isAlphanumeric(int c);

// Locale-independent character transform functions
char toLower(char c);
char toUpper(char c);

bool startsIdentifier(int c);
bool continuesIdentifier(int c);

template<uint32_t Base>
    requires(Base > 0 && Base <= 36)
bool isDigit(int c) {
	if constexpr (Base <= 10) {
		return c >= '0' && c < static_cast<int>('0' + Base);
	} else {
		return isDigit<10>(c) || (c >= 'A' && c < static_cast<int>('A' + Base - 10))
		       || (c >= 'a' && c < static_cast<int>('a' + Base - 10));
	}
}

template<uint32_t Base>
    requires(Base > 0 && Base <= 36)
uint8_t parseDigit(int c) {
	assume(isDigit<Base>(c));
	if constexpr (Base <= 10) {
		return c - '0';
	} else {
		// Check digit ranges from greatest to least ('a'-'z', then 'A'-'Z', then '0'-'9')
		if (c >= 'a') {
			return c - 'a' + 10;
		} else if (c >= 'A') {
			return c - 'A' + 10;
		} else {
			return parseDigit<10>(c);
		}
	}
}

std::optional<uint64_t> parseNumber(char const *&str, NumberBase base = BASE_AUTO);
std::optional<uint64_t> parseWholeNumber(char const *str, NumberBase base = BASE_AUTO);

char const *printChar(int c);

struct Uppercase {
	// FNV-1a hash of an uppercased string
	constexpr size_t operator()(std::string const &str) const {
		return std::accumulate(RANGE(str), size_t(0x811C9DC5), [](size_t hash, char c) {
			return (hash ^ toUpper(c)) * 16777619;
		});
	}

	// Compare two strings without case-sensitivity (by converting to uppercase)
	constexpr bool operator()(std::string const &str1, std::string const &str2) const {
		return std::equal(RANGE(str1), RANGE(str2), [](char c1, char c2) {
			return toUpper(c1) == toUpper(c2);
		});
	}
};

// An unordered map from case-insensitive `std::string` keys to `ItemT` items
template<typename ItemT>
using UpperMap = std::unordered_map<std::string, ItemT, Uppercase, Uppercase>;

#endif // RGBDS_UTIL_HPP
