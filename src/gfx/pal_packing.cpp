// SPDX-License-Identifier: MIT

#include "gfx/pal_packing.hpp"

#include <algorithm>
#include <deque>
#include <inttypes.h>
#include <iterator>
#include <numeric>
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
#include "style.hpp"
#include "verbosity.hpp"

#include "gfx/color_set.hpp"
#include "gfx/main.hpp"

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
	template<typename I, template<typename> typename Constness>
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
		I _iter{};

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

	size_t nbColorSets() const { return std::distance(RANGE(*this)); }

private:
	template<typename I>
	static void addUniqueColors(
	    std::unordered_set<uint16_t> &colors,
	    I iter,
	    I const &end,
	    std::vector<ColorSet> const &colorSets
	) {
		for (; iter != end; ++iter) {
			ColorSet const &colorSet = colorSets[iter->colorSetIndex];
			colors.insert(RANGE(colorSet));
		}
	}

	// This function should stay private because it returns a reference to a unique object
	std::unordered_set<uint16_t> &uniqueColors() const {
		// We check for *distinct* colors by stuffing them into a `set`; this should be
		// faster than "back-checking" on every element (O(n^2))
		static std::unordered_set<uint16_t> colors;

		colors.clear();
		addUniqueColors(colors, RANGE(*this), *_colorSets);
		return colors;
	}

public:
	// Returns the number of distinct colors
	size_t volume() const { return uniqueColors().size(); }

	bool canFit(ColorSet const &colorSet) const {
		std::unordered_set<uint16_t> &colors = uniqueColors();
		colors.insert(RANGE(colorSet));
		return colors.size() <= options.maxOpaqueColors();
	}

	// The `relSizeOf` method below should compute the sum, for each color in `colorSet`, of
	// the reciprocal of the "multiplicity" of the color across "our" color sets.
	// However, literally computing the reciprocals would involve floating-point division, which
	// leads to imprecision and even platform-specific differences.
	// We avoid this by multiplying the reciprocals by a factor such that division always produces
	// an integer; the LCM of all values the denominator can take is the smallest suitable factor.
	static constexpr uint32_t scaleFactor = [] {
		// Fold over 1..=17 with the associative LCM function
		// (17 is the largest the denominator in `relSizeOf` below can be)
		uint32_t factor = 1;
		for (uint32_t n = 2; n <= 17; ++n) {
			factor = std::lcm(factor, n);
		}
		return factor;
	}();

	// Computes the "relative size" of a color set on this palette;
	// it's a measure of how much this color set would "cost" to introduce.
	uint32_t relSizeOf(ColorSet const &colorSet) const {
		// NOTE: this function must not call `uniqueColors`, or one of its callers will break!

		uint32_t relSize = 0;
		for (uint16_t color : colorSet) {
			// How many of our color sets does this color also belong to?
			uint32_t multiplicity =
			    std::count_if(RANGE(*this), [this, &color](ColorSetAttrs const &attrs) {
				    ColorSet const &pal = (*_colorSets)[attrs.colorSetIndex];
				    return std::find(RANGE(pal), color) != pal.end();
			    });
			// We increase the denominator by 1 here; the reference code does this,
			// but the paper does not. Not adding 1 makes a multiplicity of 0 cause a division by 0
			// (that is, if the color is not found in any color set), and adding 1 still seems
			// to preserve the paper's reasoning.
			//
			// The scale factor should ensure integer divisions only.
			assume(scaleFactor % (multiplicity + 1) == 0);
			relSize += scaleFactor / (multiplicity + 1);
		}
		return relSize;
	}

	// Computes the "relative size" of a set of color sets on this palette
	template<typename I>
	size_t combinedVolume(I &&begin, I const &end, std::vector<ColorSet> const &colorSets) const {
		std::unordered_set<uint16_t> &colors = uniqueColors();
		addUniqueColors(colors, std::forward<I>(begin), end, colorSets);
		return colors.size();
	}

	// Computes the "relative size" of a set of colors on this palette
	template<typename I>
	size_t combinedVolume(I &&begin, I &&end) const {
		std::unordered_set<uint16_t> &colors = uniqueColors();
		colors.insert(std::forward<I>(begin), std::forward<I>(end));
		return colors.size();
	}
};

// LCOV_EXCL_START
static void verboseOutputAssignments(
    std::vector<AssignedSets> const &assignments, std::vector<ColorSet> const &colorSets
) {
	if (!checkVerbosity(VERB_INFO)) {
		return;
	}

	style_Set(stderr, STYLE_MAGENTA, false);
	for (AssignedSets const &assignment : assignments) {
		fputs("{ ", stderr);
		for (ColorSetAttrs const &attrs : assignment) {
			fprintf(stderr, "[%zu] ", attrs.colorSetIndex);
			for (uint16_t colorIndex : colorSets[attrs.colorSetIndex]) {
				fprintf(stderr, "%04" PRIx16 ", ", colorIndex);
			}
		}
		fprintf(stderr, "} (volume = %zu)\n", assignment.volume());
	}
	style_Reset(stderr);
}
// LCOV_EXCL_STOP

static void decant(std::vector<AssignedSets> &assignments, std::vector<ColorSet> const &colorSets) {
	// "Decanting" is the process of moving all *things* that can fit in a lower index there
	auto decantOn = [&assignments](auto const &tryDecanting) {
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
			// invalidating our iteration (thus, iterators should not be used to drivethe outer
			// loop)
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
		// We do this by adding the first available color set, and then looking for palettes with
		// common colors. (As an optimization, we know we can skip palettes already scanned.)
		std::vector<bool> processed(from.nbColorSets(), false);
		for (std::vector<bool>::iterator wasProcessed;
		     (wasProcessed = std::find(RANGE(processed), false)) != processed.end();) {
			auto attrs = from.begin();
			std::advance(attrs, wasProcessed - processed.begin());

			std::unordered_set<uint16_t> colors(RANGE(colorSets[attrs->colorSetIndex]));
			std::vector<size_t> members = {static_cast<size_t>(wasProcessed - processed.begin())};
			*wasProcessed = true; // Mark the first color set as processed

			// Build up the "component"...
			for (; ++wasProcessed != processed.end(); ++attrs) {
				// If at least one color matches, add it
				if (ColorSet const &colorSet = colorSets[attrs->colorSetIndex];
				    std::find_first_of(RANGE(colors), RANGE(colorSet)) != colors.end()) {
					colors.insert(RANGE(colorSet));
					members.push_back(wasProcessed - processed.begin());
					*wasProcessed = true; // Mark that color set as processed
				}
			}

			if (to.combinedVolume(RANGE(colors)) > options.maxOpaqueColors()) {
				continue;
			}

			// Iterate through the component's color sets, and transfer them
			auto member = from.begin();
			size_t curIndex = 0;
			for (size_t index : members) {
				std::advance(member, index - curIndex);
				curIndex = index;
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
	for (std::queue<ColorSetAttrs> queue(std::deque<ColorSetAttrs>(RANGE(sortedColorSetIDs)));
	     !queue.empty();
	     queue.pop()) {
		ColorSetAttrs const &attrs = queue.front(); // Valid until the `queue.pop()`
		verbosePrint(VERB_TRACE, "Handling color set %zu\n", attrs.colorSetIndex);

		ColorSet const &colorSet = colorSets[attrs.colorSetIndex];
		size_t bestPalIndex = assignments.size();
		// We're looking for a palette where the color set's relative size is less than
		// its actual size; so only overwrite the "not found" index on meeting that criterion
		uint32_t bestRelSize = colorSet.size() * AssignedSets::scaleFactor;

		for (size_t i = 0; i < assignments.size(); ++i) {
			// Skip the page if this one is banned from it
			if (attrs.isBannedFrom(i)) {
				continue;
			}

			uint32_t relSize = assignments[i].relSizeOf(colorSet);
			verbosePrint(
			    VERB_TRACE,
			    "  Relative size to palette %zu (of %zu): %" PRIu32 " (size = %zu)\n",
			    i,
			    assignments.size(),
			    relSize,
			    colorSet.size()
			);
			if (relSize < bestRelSize) {
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
			uint32_t relSize1 = bestPal.relSizeOf(colorSet1);
			uint32_t relSize2 = bestPal.relSizeOf(colorSet2);

			verbosePrint(
			    VERB_TRACE,
			    "  Color sets %zu <=> %zu: Efficiency: %zu / %" PRIu32 " <=> %zu / "
			    "%" PRIu32 "\n",
			    attrs1.colorSetIndex,
			    attrs2.colorSetIndex,
			    size1,
			    relSize1,
			    size2,
			    relSize2
			);

			// This comparison is algebraically equivalent to
			// `size1 / relSize1 <=> size2 / relSize2`,
			// but without potential precision loss from floating-point division.
			size_t efficiency1 = size1 * relSize2;
			size_t efficiency2 = size2 * relSize1;
			return (efficiency1 > efficiency2) - (efficiency1 < efficiency2);
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

	verboseOutputAssignments(assignments, colorSets); // LCOV_EXCL_LINE

	// "Decant" the result
	decant(assignments, colorSets);
	// Note that the result does not contain any empty palettes

	verboseOutputAssignments(assignments, colorSets); // LCOV_EXCL_LINE

	std::vector<size_t> mappings(colorSets.size());
	for (size_t i = 0; i < assignments.size(); ++i) {
		for (ColorSetAttrs const &attrs : assignments[i]) {
			mappings[attrs.colorSetIndex] = i;
		}
	}
	return {mappings, assignments.size()};
}
