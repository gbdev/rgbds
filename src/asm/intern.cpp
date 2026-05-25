// SPDX-License-Identifier: MIT

#include "asm/intern.hpp"

#include <deque>
#include <functional> // equal_to
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>

#include "helpers.hpp" // assume
#include "verbosity.hpp"

// Avoid `std::string` allocations when looking up heterogeneous values in `internedIndexes`
struct StringHash {
	using is_transparent = void;

	size_t operator()(std::string_view str) const { return std::hash<std::string_view>{}(str); }
};

// Use a `deque` not a `vector` to prevent reallocation so `internedIndexes` keys stay valid
static std::deque<std::string> internedStrings;
// Keys are views of values in `internedStrings`; values are their corresponding indexes
static std::unordered_map<std::string_view, size_t, StringHash, std::equal_to<>> internedIndexes;

std::string const &InternedStr::str() const {
	assume(index != static_cast<size_t>(-1));
	return internedStrings[index];
}

InternedStr intern(std::string_view str) {
	if (auto search = internedIndexes.find(str); search != internedIndexes.end()) {
		return InternedStr(search->second);
	}

	size_t index = internedStrings.size();
	std::string &interned = internedStrings.emplace_back(str);
	internedIndexes.emplace(interned, index);

	verbosePrint(VERB_TRACE, "Interned string \"%s\"\n", interned.c_str());

	return InternedStr(index);
}
