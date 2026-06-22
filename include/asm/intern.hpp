// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_INTERN_HPP
#define RGBDS_ASM_INTERN_HPP

#include <stddef.h>
#include <string>
#include <string_view>
#include <utility> // hash

class InternedStr {
	size_t index;

public:
	constexpr InternedStr() : index(static_cast<size_t>(-1)) {}
	explicit constexpr InternedStr(size_t index_) : index(index_) {}

	std::string const &str() const;
	char const *c_str() const { return str().c_str(); }

	bool operator==(InternedStr const &rhs) const { return index == rhs.index; }

	template<typename T>
	friend struct std::hash;
};

template<>
struct std::hash<InternedStr> {
	size_t operator()(InternedStr const &str) const { return std::hash<size_t>{}(str.index); }
};

InternedStr intern(std::string_view str);

#endif // RGBDS_ASM_INTERN_HPP
