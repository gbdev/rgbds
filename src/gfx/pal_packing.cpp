// SPDX-License-Identifier: MIT

#include "gfx/pal_packing.hpp"

#include <algorithm>
#include <inttypes.h>
#include <iterator>
#include <optional>
#include <queue>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "helpers.hpp"
#include "verbosity.hpp"

#include "gfx/color_set.hpp"
#include "gfx/main.hpp"
#include "gfx/warning.hpp"

// The solvers here are picked from the paper at https://arxiv.org/abs/1605.00558:
// "Algorithms for the Pagination Problem, a Bin Packing with Overlapping Items"
// Their formulation of the problem consists in packing "tiles" into "pages".
// Here is a correspondence table for our application of it:
//
// Paper  | RGBGFX
// -------+----------
// Symbol | Color
// Tile   | Color set
// Page   | Palette

static constexpr uint64_t ipow(uint64_t base, size_t power) {
	uint64_t result = 1;
	while (power > 0) {
		result *= base;
		--power;
	}
	return result;
}

struct Fraction {
	uint64_t numer, denom;

	uint64_t quotient() const {
		assume(denom != 0);
		return numer / denom;
	}

	uint64_t remainder() const {
		assume(denom != 0);
		return numer % denom;
	}

	Fraction reciprocal() const {
		assume(numer != 0);
		return {denom, numer};
	}

	Fraction scale(uint64_t n) const {
		assume(numer <= UINT64_MAX / n);
		assume(denom <= UINT64_MAX / n);
		return {numer * n, denom * n};
	}

	Fraction add_numerator(uint64_t n) const {
		assume(numer <= UINT64_MAX - n);
		return {numer + n, denom};
	}

	Fraction mul(uint64_t n) const {
		assume(numer <= UINT64_MAX / n);
		return {numer * n, denom};
	}

	int compare(Fraction rhs) const {
		Fraction lhs = *this;
		// Compare two fractions by comparing terms of their continued fraction representations.
		// For a fraction N/D, where Q = floor(N / D) and R = N % D, N/D = Q + R/D = Q + 1/(D/R),
		// a process which can be continued for the fraction D/R.
		for (int sign = 1;;) {
			if (uint64_t l_quo = lhs.quotient(), r_quo = rhs.quotient(); l_quo != r_quo) {
				// If the integer quotients Q and Q' are different, then they can be compared
				// immediately, ignoring the remainders R/D and R'/D' which are smaller than 1.
				return l_quo < r_quo ? -sign : sign;
			} else if (uint64_t l_rem = lhs.remainder(), r_rem = rhs.remainder();
			           l_rem == 0 || r_rem == 0) {
				// Otherwise, if either remainder R or R' is zero, then either continued fraction
				// halts, and zero versus nonzero remainders can be compared immediately.
				return l_rem == 0 ? r_rem == 0 ? 0 : -sign : sign;
			} else {
				// Otherwise, to compare remainders 1/(D/R) and 1/(D'/R'), take the reciprocals
				// R/D and R'/D' to allow further reduction (which flips the comparison result).
				// Since R < D and R' < D', the next Q and Q' will be nonzero, thus the continued
				// fraction terms keep decreasing and this loop inductively terminates.
				lhs = {lhs.denom, l_rem};
				rhs = {rhs.denom, r_rem};
				sign = -sign;
			}
		}
	}
};

// A valid palette with N opaque colors can have at most 2^N - 1 distinct nonempty color sets.
// `options.maxOpaqueColors()` is at most `MAX_PAL_COLORS` (which is 4, since `options.bitDepth`
// is at most 2), and `options.nbPalettes` is at most `MAX_NB_PALETTES` (256), so there can be
// at most `MAX_NB_PALETTES * (2^MAX_PAL_COLORS - 1)` == 3840 distinct color sets.
static constexpr size_t MAX_NB_COLOR_SETS = MAX_NB_PALETTES * ((1u << MAX_PAL_COLORS) - 1);

// A reference to a color set, and attached attributes for sorting purposes
struct ColorSetAttrs {
	size_t colorSetIndex;
	// Pages from which we are banned (to prevent infinite loops)
	// This is dynamic because we wish not to hard-cap the amount of palettes
	std::vector<bool> bannedPages;

	explicit ColorSetAttrs(size_t index) : colorSetIndex(index) {}

	bool isBannedFrom(size_t index) const {
		return index < bannedPages.size() && bannedPages[index];
	}

	void banFrom(size_t index) {
		if (bannedPages.size() <= index) {
			bannedPages.resize(index + 1);
		}
		bannedPages[index] = true;
	}
};

// A collection of color sets assigned to a palette
// Does not contain the actual color indices because we need to be able to remove elements
class AssignedSets {
	// We leave room for emptied slots to avoid copying the structs around on removal
	std::vector<std::optional<ColorSetAttrs>> _assigned;
	// For resolving color set indices
	std::vector<ColorSet> const *_colorSets;

public:
	AssignedSets(std::vector<ColorSet> const &colorSets, std::optional<ColorSetAttrs> &&attrs)
	    : _assigned{attrs}, _colorSets{&colorSets} {}

private:
	// Template class for both const and non-const iterators over the non-empty `_assigned` slots
	template<typename IteratorT, template<typename> typename Constness>
	class AssignedSetsIter {
	public:
		friend class AssignedSets;

		// For `iterator_traits`
		using value_type = ColorSetAttrs;
		using difference_type = ptrdiff_t;
		using reference = Constness<value_type> &;
		using pointer = Constness<value_type> *;
		using iterator_category = std::forward_iterator_tag;

	private:
		Constness<decltype(_assigned)> *_array = nullptr;
		IteratorT _iter{};

		AssignedSetsIter(decltype(_array) array, decltype(_iter) &&iter)
		    : _array(array), _iter(iter) {}

		AssignedSetsIter &skipEmpty() {
			while (_iter != _array->end() && !_iter->has_value()) {
				++_iter;
			}
			return *this;
		}

	public:
		AssignedSetsIter() = default;

		bool operator==(AssignedSetsIter const &rhs) const { return _iter == rhs._iter; }

		AssignedSetsIter &operator++() {
			++_iter;
			skipEmpty();
			return *this;
		}
		AssignedSetsIter operator++(int) {
			AssignedSetsIter it = *this;
			++(*this);
			return it;
		}

		reference operator*() const {
			assume((*_iter).has_value());
			return **_iter;
		}
		pointer operator->() const {
			return &(**this); // Invokes the operator above, not quite a no-op!
		}

		friend void swap(AssignedSetsIter &lhs, AssignedSetsIter &rhs) {
			std::swap(lhs._array, rhs._array);
			std::swap(lhs._iter, rhs._iter);
		}
	};

public:
	using iterator = AssignedSetsIter<decltype(_assigned)::iterator, std::remove_const_t>;
	iterator begin() { return iterator{&_assigned, _assigned.begin()}.skipEmpty(); }
	iterator end() { return iterator{&_assigned, _assigned.end()}; }

	using const_iterator = AssignedSetsIter<decltype(_assigned)::const_iterator, std::add_const_t>;
	const_iterator begin() const {
		return const_iterator{&_assigned, _assigned.begin()}.skipEmpty();
	}
	const_iterator end() const { return const_iterator{&_assigned, _assigned.end()}; }

	iterator slotAt(size_t index) { return iterator{&_assigned, _assigned.begin() + index}; }

	void assign(ColorSetAttrs const &&attrs) {
		auto freeSlot =
		    std::find_if_not(RANGE(_assigned), [](std::optional<ColorSetAttrs> const &slot) {
			    return slot.has_value();
		    });
		if (freeSlot == _assigned.end()) {
			_assigned.emplace_back(attrs); // We are full, use a new slot
		} else {
			freeSlot->emplace(attrs); // Reuse a free slot
		}
	}

	void remove(iterator const &iter) {
		iter._iter->reset(); // This time, we want to access the `optional` itself
	}

	void clear() { _assigned.clear(); }

	bool empty() const {
		return std::none_of(RANGE(_assigned), [](std::optional<ColorSetAttrs> const &slot) {
			return slot.has_value();
		});
	}

	size_t nbSlots() const { return _assigned.size(); }

	bool isFree(size_t slotIndex) const { return !_assigned[slotIndex].has_value(); }

private:
	template<typename IteratorT>
	static void addUniqueColors(
	    std::unordered_set<uint16_t> &colors,
	    IteratorT iter,
	    IteratorT const &end,
	    std::vector<ColorSet> const &colorSets
	) {
		for (; iter != end; ++iter) {
			ColorSet const &colorSet = colorSets[iter->colorSetIndex];
			colors.insert(RANGE(colorSet));
		}
	}

public:
	// Returns the set of distinct colors
	std::unordered_set<uint16_t> uniqueColors() const {
		std::unordered_set<uint16_t> colors;
		addUniqueColors(colors, RANGE(*this), *_colorSets);
		return colors;
	}

	// Returns the number of distinct colors
	size_t volume() const { return uniqueColors().size(); }

	bool canFit(ColorSet const &colorSet) const {
		std::unordered_set<uint16_t> colors = uniqueColors();
		colors.insert(RANGE(colorSet));
		return colors.size() <= options.maxOpaqueColors();
	}

	// Counts how many of our color sets this color also belongs to
	uint32_t multiplicity(uint16_t color) const {
		return std::count_if(RANGE(*this), [this, &color](ColorSetAttrs const &attrs) {
			ColorSet const &pal = (*_colorSets)[attrs.colorSetIndex];
			return std::find(RANGE(pal), color) != pal.end();
		});
	}

	// Computes the "relative size" of a color set on this palette.
	// It's a measure of how much this color set would "cost" to introduce: the sum, for each color
	// in `colorSet`, of the reciprocal of the "multiplicity" of the color across "our" color sets.
	// However, literally computing the reciprocals would involve floating-point division, which
	// leads to imprecision and even platform-specific differences.
	// We avoid this by returning an unreduced fraction.
	Fraction relSizeOf(ColorSet const &colorSet) const {
		static constexpr size_t MAX_N = MAX_NB_COLOR_SETS + 1;
		// For a color set with the maximum number of colors (`MAX_PAL_COLORS` == 4), and each color
		// having the maximum "multiplicity" (`MAX_N` == `MAX_NB_COLOR_SETS + 1` == 3841), the
		// "relative size" calculation would add MAX_PAL_COLORS copies of 1/MAX_N to 0/1, giving a
		// reduced fraction of MAX_PAL_COLORS/MAX_N.
		// In the algebraically equivalent calculation below, the maximum numerator would be
		// MAX_PAL_COLORS * MAX_N^(MAX_PAL_COLORS - 1) == 226669409284, and the maximum denominator
		// would be MAX_N^MAX_PAL_COLORS == 217659300264961, both of which fit in `uint64_t`.
		assume(colorSet.size() <= MAX_PAL_COLORS);
		static_assert(ipow(MAX_N, MAX_PAL_COLORS - 1) <= UINT64_MAX / MAX_PAL_COLORS);
		static_assert(ipow(MAX_N, MAX_PAL_COLORS) <= UINT64_MAX);

		Fraction relSize{0, 1};
		for (uint16_t color : colorSet) {
			// We increase the denominator by 1 here; the reference code does this,
			// but the paper does not. Not adding 1 makes a multiplicity of 0 cause a division by 0
			// (that is, if the color is not found in any color set), and adding 1 still seems
			// to preserve the paper's reasoning.
			uint64_t n = multiplicity(color) + 1;
			assume(n <= MAX_N);
			// Add 1/n to the relative size by first scaling the numerator and denominator by n.
			// This can leave the fraction in a reducible state, but there's no need to reduce it
			// since unreduced quantities will still fit in `uint64_t`.
			relSize = relSize.scale(n).add_numerator(relSize.denom);
		}
		return relSize;
	}

	// Computes the "relative size" of a set of color sets on this palette
	template<typename IteratorT>
	size_t combinedVolume(
	    IteratorT &&begin, IteratorT const &end, std::vector<ColorSet> const &colorSets
	) const {
		std::unordered_set<uint16_t> colors = uniqueColors();
		addUniqueColors(colors, std::forward<IteratorT>(begin), end, colorSets);
		return colors.size();
	}

	// Computes the "relative size" of a set of colors on this palette
	template<typename IteratorT>
	size_t combinedVolume(IteratorT &&begin, IteratorT &&end) const {
		std::unordered_set<uint16_t> colors = uniqueColors();
		colors.insert(std::forward<IteratorT>(begin), std::forward<IteratorT>(end));
		return colors.size();
	}
};

// LCOV_EXCL_START
static void verboseOutputAssignments(
    std::vector<AssignedSets> const &assignments, std::vector<ColorSet> const &colorSets
) {
	verboseDo(VERB_INFO, [&]() {
		for (AssignedSets const &assignment : assignments) {
			fputs("- { ", stderr);
			for (ColorSetAttrs const &attrs : assignment) {
				fprintf(
				    stderr,
				    "[%zu] %s ",
				    attrs.colorSetIndex,
				    listCGBColors(colorSets[attrs.colorSetIndex]).c_str()
				);
			}
			fprintf(stderr, "} (volume = %zu)\n", assignment.volume());
		}
	});
}
// LCOV_EXCL_STOP

static void decant(std::vector<AssignedSets> &assignments, std::vector<ColorSet> const &colorSets) {
	assume(!assignments.empty());

	// "Decanting" is the process of moving all *things* that can fit in a lower index there
	auto decantOn = [&assignments](Procedure<AssignedSets &, AssignedSets &> auto tryDecanting) {
		// No need to attempt decanting on palette #0, as there are no palettes to decant to
		for (size_t from = assignments.size(); --from;) {
			// Scan all palettes before this one
			for (size_t to = 0; to < from; ++to) {
				tryDecanting(assignments[to], assignments[from]);
			}

			// If the color set is now empty, remove it
			// Doing this now reduces the number of iterations performed by later steps
			// NB: order is intentionally preserved so as not to alter the "decantation"'s
			// properties
			// NB: this does mean that the first step might get empty palettes as its input!
			// NB: this is safe to do because we go towards the beginning of the vector, thereby not
			// invalidating our iteration (so iterators should not be used to drive the outer loop)
			if (assignments[from].empty()) {
				assignments.erase(assignments.begin() + from);
			}
		}
	};

	verbosePrint(VERB_DEBUG, "%zu palettes before decanting\n", assignments.size());

	// Decant on palettes
	decantOn([&colorSets](AssignedSets &to, AssignedSets &from) {
		// If the entire palettes can be merged, move all of `from`'s color sets
		if (to.combinedVolume(RANGE(from), colorSets) <= options.maxOpaqueColors()) {
			for (ColorSetAttrs &attrs : from) {
				to.assign(std::move(attrs));
			}
			from.clear();
		}
	});
	verbosePrint(VERB_DEBUG, "%zu palettes after decanting on palettes\n", assignments.size());

	// Decant on "components" (color sets sharing colors)
	decantOn([&colorSets](AssignedSets &to, AssignedSets &from) {
		// We need to iterate on all the "components", which are groups of color sets sharing at
		// least one color with another color set in the group.
		// We do this by adding the first free color set, and then looking for palettes with
		// common colors. (As an optimization, we know we can skip palettes already processed.)
		std::vector<bool> processed(from.nbSlots(), false);
		for (size_t startIdx = 0; startIdx < processed.size(); ++startIdx) {
			if (processed[startIdx] || from.isFree(startIdx)) {
				continue;
			}
			size_t startColorSetIdx = from.slotAt(startIdx)->colorSetIndex;

			// Build up the "component"; start by marking the first color set as processed
			std::unordered_set<uint16_t> colors(RANGE(colorSets[startColorSetIdx]));
			std::vector<size_t> members = {startIdx};
			processed[startIdx] = true;
			for (size_t nextIdx = startIdx + 1; nextIdx < processed.size(); ++nextIdx) {
				if (processed[nextIdx] || from.isFree(nextIdx)) {
					continue;
				}
				size_t nextColorSetIdx = from.slotAt(nextIdx)->colorSetIndex;
				// If at least one color matches, add it
				if (ColorSet const &colorSet = colorSets[nextColorSetIdx];
				    std::find_first_of(RANGE(colors), RANGE(colorSet)) != colors.end()) {
					colors.insert(RANGE(colorSet));
					members.push_back(nextIdx);
					processed[nextIdx] = true; // Mark the added color set as processed
				}
			}

			if (to.combinedVolume(RANGE(colors)) > options.maxOpaqueColors()) {
				continue;
			}

			// Iterate through the component's color sets, and transfer them
			for (size_t index : members) {
				auto member = from.slotAt(index);
				to.assign(std::move(*member));
				from.remove(member); // Removing does not shift elements, so it's cheap
			}
		}
	});
	verbosePrint(
	    VERB_DEBUG, "%zu palettes after decanting on \"components\"\n", assignments.size()
	);

	// Decant on individual color sets
	decantOn([&colorSets](AssignedSets &to, AssignedSets &from) {
		for (auto it = from.begin(); it != from.end(); ++it) {
			if (to.canFit(colorSets[it->colorSetIndex])) {
				to.assign(std::move(*it));
				from.remove(it);
			}
		}
	});
	verbosePrint(VERB_DEBUG, "%zu palettes after decanting on color sets\n", assignments.size());
}

std::pair<std::vector<size_t>, size_t> overloadAndRemove(std::vector<ColorSet> const &colorSets) {
	verbosePrint(VERB_NOTICE, "Paginating palettes using \"overload-and-remove\" strategy...\n");

	// Color set generation in `process` already avoided making duplicate or subset color sets,
	// so we can limit the maximum number of color sets here to prevent `Fraction` overflow.
	if (colorSets.size() > MAX_NB_COLOR_SETS) {
		fatal(
		    "Too many distinct tile color combinations for %" PRIu16 " palette%s",
		    options.nbPalettes,
		    options.nbPalettes == 1 ? "" : "s"
		);
	}

	// Sort the color sets by size, which improves the packing algorithm's efficiency
	auto const indexOfLargestColorSetFirst = [&colorSets](size_t left, size_t right) {
		ColorSet const &lhs = colorSets[left];
		ColorSet const &rhs = colorSets[right];
		return lhs.size() > rhs.size(); // We want the color sets to be sorted *largest first*!
	};
	std::vector<size_t> sortedColorSetIDs;
	sortedColorSetIDs.reserve(colorSets.size());
	for (size_t i = 0; i < colorSets.size(); ++i) {
		sortedColorSetIDs.insert(
		    std::lower_bound(RANGE(sortedColorSetIDs), i, indexOfLargestColorSetFirst), i
		);
	}

	// Begin with no pages
	std::vector<AssignedSets> assignments;

	// Begin with all color sets queued up for insertion
	for (std::queue<ColorSetAttrs> queue({RANGE(sortedColorSetIDs)}); !queue.empty(); queue.pop()) {
		ColorSetAttrs const &attrs = queue.front(); // Valid until the `queue.pop()`
		verbosePrint(VERB_TRACE, "Handling color set %zu\n", attrs.colorSetIndex);

		ColorSet const &colorSet = colorSets[attrs.colorSetIndex];
		size_t bestPalIndex = assignments.size();
		// We're looking for a palette where the color set's relative size is less than
		// its actual size; so only overwrite the "not found" index on meeting that criterion
		Fraction bestRelSize{colorSet.size(), 1};

		for (size_t i = 0; i < assignments.size(); ++i) {
			// Skip the page if this one is banned from it
			if (attrs.isBannedFrom(i)) {
				continue;
			}

			Fraction relSize = assignments[i].relSizeOf(colorSet);
			verbosePrint(
			    VERB_TRACE,
			    "  Relative size to palette %zu (of %zu): %" PRIu64 "/%" PRIu64 " (size = %zu)\n",
			    i,
			    assignments.size(),
			    relSize.numer,
			    relSize.denom,
			    colorSet.size()
			);
			if (relSize.compare(bestRelSize) < 0) {
				bestPalIndex = i;
				bestRelSize = relSize;
			}
		}

		if (bestPalIndex == assignments.size()) {
			// Found nowhere to put it, create a new page containing just that one
			verbosePrint(
			    VERB_TRACE,
			    "Assigning color set %zu to new palette %zu\n",
			    attrs.colorSetIndex,
			    bestPalIndex
			);
			assignments.emplace_back(colorSets, std::move(attrs));
			continue;
		}

		verbosePrint(
		    VERB_TRACE,
		    "Assigning color set %zu to palette %zu\n",
		    attrs.colorSetIndex,
		    bestPalIndex
		);
		AssignedSets &bestPal = assignments[bestPalIndex];
		// Add the color to that palette
		bestPal.assign(std::move(attrs));

		auto compareEfficiency = [&](ColorSetAttrs const &attrs1, ColorSetAttrs const &attrs2) {
			ColorSet const &colorSet1 = colorSets[attrs1.colorSetIndex];
			ColorSet const &colorSet2 = colorSets[attrs2.colorSetIndex];
			size_t size1 = colorSet1.size();
			size_t size2 = colorSet2.size();
			Fraction relSize1 = bestPal.relSizeOf(colorSet1);
			Fraction relSize2 = bestPal.relSizeOf(colorSet2);
			// For a color set with the maximum number of colors (`MAX_PAL_COLORS` == 4), and the
			// maximum relative size (denominator == `(MAX_NB_COLOR_SETS + 1)^MAX_PAL_COLORS` ==
			// 217659300264961), the maximum efficiency numerator would be `MAX_PAL_COLORS *
			// (MAX_NB_COLOR_SETS + 1)^MAX_PAL_COLORS` == 870637201059844, which fits in `uint64_t`.
			static_assert(
			    ipow(MAX_NB_COLOR_SETS + 1, MAX_PAL_COLORS) <= UINT64_MAX / MAX_PAL_COLORS
			);
			// efficiency == size / relSize == size * 1 / relSize
			Fraction efficiency1 = relSize1.reciprocal().mul(size1);
			Fraction efficiency2 = relSize2.reciprocal().mul(size2);
			verbosePrint(
			    VERB_TRACE,
			    "  Color sets %zu <=> %zu: Efficiency: %" PRIu64 "/%" PRIu64 " <=> %" PRIu64
			    "/%" PRIu64 "\n",
			    attrs1.colorSetIndex,
			    attrs2.colorSetIndex,
			    efficiency1.numer,
			    efficiency1.denom,
			    efficiency2.numer,
			    efficiency2.denom
			);
			return efficiency1.compare(efficiency2);
		};

		// If this overloads the palette, get it back to normal (if possible)
		while (bestPal.volume() > options.maxOpaqueColors()) {
			verbosePrint(
			    VERB_TRACE,
			    "Palette %zu is overloaded! (%zu > %" PRIu8 ")\n",
			    bestPalIndex,
			    bestPal.volume(),
			    options.maxOpaqueColors()
			);

			// Look for a color set minimizing "efficiency" (size / relSize)
			auto [minEfficiencyIter, maxEfficiencyIter] = std::minmax_element(
			    RANGE(bestPal),
			    [&compareEfficiency](ColorSetAttrs const &lhs, ColorSetAttrs const &rhs) {
				    return compareEfficiency(lhs, rhs) < 0;
			    }
			);

			// All efficiencies are identical iff min equals max
			if (compareEfficiency(*minEfficiencyIter, *maxEfficiencyIter) == 0) {
				verbosePrint(VERB_TRACE, "  All efficiencies are identical\n");
				break;
			}

			// Remove the color set with minimal efficiency
			verbosePrint(
			    VERB_TRACE, "  Removing color set %zu\n", minEfficiencyIter->colorSetIndex
			);
			queue.emplace(std::move(*minEfficiencyIter));
			queue.back().banFrom(bestPalIndex); // Ban it from this palette
			bestPal.remove(minEfficiencyIter);
		}
	}

	// Deal with palettes still overloaded, by emptying them
	auto const &largestColorSetFirst =
	    [&colorSets](ColorSetAttrs const &lhs, ColorSetAttrs const &rhs) {
		    return colorSets[lhs.colorSetIndex].size() > colorSets[rhs.colorSetIndex].size();
	    };
	std::vector<ColorSetAttrs> overloadQueue{};
	for (AssignedSets &pal : assignments) {
		if (pal.volume() > options.maxOpaqueColors()) {
			for (ColorSetAttrs &attrs : pal) {
				overloadQueue.emplace(
				    std::lower_bound(RANGE(overloadQueue), attrs, largestColorSetFirst),
				    std::move(attrs)
				);
			}
			pal.clear();
		}
	}
	// Place back any color sets now in the queue via first-fit
	for (ColorSetAttrs const &attrs : overloadQueue) {
		ColorSet const &colorSet = colorSets[attrs.colorSetIndex];
		auto palette = std::find_if(RANGE(assignments), [&colorSet](AssignedSets const &pal) {
			return pal.canFit(colorSet);
		});
		if (palette == assignments.end()) { // No such page, create a new one
			verbosePrint(
			    VERB_DEBUG,
			    "Adding new palette (%zu) for overflowing color set %zu\n",
			    assignments.size(),
			    attrs.colorSetIndex
			);
			assignments.emplace_back(colorSets, std::move(attrs));
		} else {
			verbosePrint(
			    VERB_DEBUG,
			    "Assigning overflowing color set %zu to palette %zu\n",
			    attrs.colorSetIndex,
			    palette - assignments.begin()
			);
			palette->assign(std::move(attrs));
		}
	}

	verboseOutputAssignments(assignments, colorSets);

	// "Decant" the result
	decant(assignments, colorSets);
	// Note that the result does not contain any empty palettes

	verboseOutputAssignments(assignments, colorSets);

	std::vector<size_t> mappings(colorSets.size());
	for (size_t i = 0; i < assignments.size(); ++i) {
		for (ColorSetAttrs const &attrs : assignments[i]) {
			mappings[attrs.colorSetIndex] = i;
		}
	}
	return {mappings, assignments.size()};
}
