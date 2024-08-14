/* SPDX-License-Identifier: MIT */

#include "gfx/proto_palette.hpp"

#include <algorithm>

#include "helpers.hpp"

bool ProtoPalette::add(uint16_t color) {
	size_t i = 0;

	// Seek the first slot greater than the new color
	// (A linear search is better because we don't store the array size,
	// and there are very few slots anyway)
	while (_colorIndices[i] < color) {
		++i;
		if (i == _colorIndices.size()) {
			// We reached the end of the array without finding the color, so it's a new one.
			return true;
		}
	}
	// If we found it, great! Nothing else to do.
	if (_colorIndices[i] == color) {
		return false;
	}

	// Swap entries until the end
	while (_colorIndices[i] != UINT16_MAX) {
		std::swap(_colorIndices[i], color);
		++i;
		if (i == _colorIndices.size()) {
			// The set is full, but doesn't include the new color.
			return true;
		}
	}
	// Write that last one into the new slot
	_colorIndices[i] = color;
	return true;
}

ProtoPalette::ComparisonResult ProtoPalette::compare(ProtoPalette const &other) const {
	// This works because the sets are sorted numerically
	assume(std::is_sorted(RANGE(_colorIndices)));
	assume(std::is_sorted(RANGE(other._colorIndices)));

	auto ours = _colorIndices.begin(), theirs = other._colorIndices.begin();
	bool weBigger = true, theyBigger = true;

	while (ours != end() && theirs != other.end()) {
		if (*ours == *theirs) {
			++ours;
			++theirs;
		} else if (*ours < *theirs) {
			++ours;
			theyBigger = false;
		} else { // *ours > *theirs
			++theirs;
			weBigger = false;
		}
	}
	weBigger &= theirs == other.end();
	theyBigger &= ours == end();

	return theyBigger ? THEY_BIGGER : (weBigger ? WE_BIGGER : NEITHER);
}

size_t ProtoPalette::size() const {
	return std::distance(RANGE(*this));
}

bool ProtoPalette::empty() const {
	return _colorIndices[0] == UINT16_MAX;
}

auto ProtoPalette::begin() const -> decltype(_colorIndices)::const_iterator {
	return _colorIndices.begin();
}
auto ProtoPalette::end() const -> decltype(_colorIndices)::const_iterator {
	return std::find(RANGE(_colorIndices), UINT16_MAX);
}
