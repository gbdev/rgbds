// SPDX-License-Identifier: MIT

#include "gfx/color_set.hpp"

#include <algorithm>
#include <iterator>
#include <stdint.h>
#include <stdlib.h>
#include <utility>

#include "helpers.hpp"

void ColorSet::add(uint16_t color) {
	size_t i = 0;

	// Seek the first slot greater than the new color
	// (A linear search is better because we don't store the array size,
	// and there are very few slots anyway)
	while (_colorIndices[i] < color) {
		++i;
		if (i == _colorIndices.size()) {
			// We reached the end of the array without finding the color, so it's a new one.
			return;
		}
	}
	// If we found it, great! Nothing else to do.
	if (_colorIndices[i] == color) {
		return;
	}

	// Swap entries until the end
	while (_colorIndices[i] != UINT16_MAX) {
		std::swap(_colorIndices[i], color);
		++i;
		if (i == _colorIndices.size()) {
			// The set is full, but doesn't include the new color.
			return;
		}
	}
	// Write that last one into the new slot
	_colorIndices[i] = color;
}

ColorSet::ComparisonResult ColorSet::compare(ColorSet const &other) const {
	// This algorithm works because the sets are sorted numerically
	assume(std::is_sorted(RANGE(_colorIndices)));
	assume(std::is_sorted(RANGE(other._colorIndices)));

	auto self_item = begin(), other_item = other.begin();
	auto const self_end = end(), other_end = other.end();
	bool self_has_unique = false, other_has_unique = false;

	while (self_item != self_end && other_item != other_end) {
		if (*self_item < *other_item) {
			// *self_item is not in other, so self cannot be a strict subset of other
			self_has_unique = true;
			++self_item;
		} else if (*self_item > *other_item) {
			// *other_item is not in self, so self cannot be a strict superset of other
			other_has_unique = true;
			++other_item;
		} else {
			// *self_item == *other_item, so continue comparing
			++self_item;
			++other_item;
		}

		// Early return optimization: we already know self and other are incomparable
		if (self_has_unique && other_has_unique) {
			return INCOMPARABLE;
		}
	}

	if (self_item != self_end) {
		// self has more unique items after other has been fully iterated, so either self
		// is a strict superset, or they're incomparable if other had unique items earlier
		assume(other_item == other_end); // necessary to have exited the `while` loop
		return other_has_unique ? INCOMPARABLE : STRICT_SUPERSET;
	} else {
		// self has been fully iterated (whether or not other has been), so either other
		// is a strict superset, or they're incomparable if self had unique items earlier
		return self_has_unique ? INCOMPARABLE : SUBSET_OR_EQUAL;
	}
}

size_t ColorSet::size() const {
	return std::distance(RANGE(*this));
}

bool ColorSet::empty() const {
	return _colorIndices[0] == UINT16_MAX;
}

auto ColorSet::begin() const -> decltype(_colorIndices)::const_iterator {
	return _colorIndices.begin();
}
auto ColorSet::end() const -> decltype(_colorIndices)::const_iterator {
	return std::find(RANGE(_colorIndices), UINT16_MAX);
}
