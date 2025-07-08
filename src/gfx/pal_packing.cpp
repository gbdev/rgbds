// SPDX-License-Identifier: MIT

#include "gfx/pal_packing.hpp"

#include <algorithm>
#include <deque>
#include <inttypes.h>
#include <numeric>
#include <optional>
#include <queue>
#include <stdint.h>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "helpers.hpp"

#include "gfx/main.hpp"
#include "gfx/proto_palette.hpp"

// The solvers here are picked from the paper at https://arxiv.org/abs/1605.00558:
// "Algorithms for the Pagination Problem, a Bin Packing with Overlapping Items"
// Their formulation of the problem consists in packing "tiles" into "pages"; here is a
// correspondence table for our application of it:
// Paper | RGBGFX
// ------+-------
//  Tile | Proto-palette
//  Page | Palette

// A reference to a proto-palette, and attached attributes for sorting purposes
struct ProtoPalAttrs {
	size_t protoPalIndex;
	// Pages from which we are banned (to prevent infinite loops)
	// This is dynamic because we wish not to hard-cap the amount of palettes
	std::vector<bool> bannedPages;

	explicit ProtoPalAttrs(size_t index) : protoPalIndex(index) {}
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

// A collection of proto-palettes assigned to a palette
// Does not contain the actual color indices because we need to be able to remove elements
class AssignedProtos {
	// We leave room for emptied slots to avoid copying the structs around on removal
	std::vector<std::optional<ProtoPalAttrs>> _assigned;
	// For resolving proto-palette indices
	std::vector<ProtoPalette> const *_protoPals;

public:
	template<typename... Ts>
	AssignedProtos(std::vector<ProtoPalette> const &protoPals, Ts &&...elems)
	    : _assigned{std::forward<Ts>(elems)...}, _protoPals{&protoPals} {}

private:
	template<typename Inner, template<typename> typename Constness>
	class Iter {
	public:
		friend class AssignedProtos;
		// For `iterator_traits`
		using difference_type = typename std::iterator_traits<Inner>::difference_type;
		using value_type = ProtoPalAttrs;
		using pointer = Constness<value_type> *;
		using reference = Constness<value_type> &;
		using iterator_category = std::forward_iterator_tag;

	private:
		Constness<decltype(_assigned)> *_array = nullptr;
		Inner _iter{};

		Iter(decltype(_array) array, decltype(_iter) &&iter) : _array(array), _iter(iter) {}
		Iter &skipEmpty() {
			while (_iter != _array->end() && !_iter->has_value()) {
				++_iter;
			}
			return *this;
		}

	public:
		Iter() = default;

		bool operator==(Iter const &rhs) const { return _iter == rhs._iter; }

		Iter &operator++() {
			++_iter;
			skipEmpty();
			return *this;
		}
		Iter operator++(int) {
			Iter it = *this;
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

		friend void swap(Iter &lhs, Iter &rhs) {
			std::swap(lhs._array, rhs._array);
			std::swap(lhs._iter, rhs._iter);
		}
	};
public:
	using iterator = Iter<decltype(_assigned)::iterator, std::remove_const_t>;
	iterator begin() { return iterator{&_assigned, _assigned.begin()}.skipEmpty(); }
	iterator end() { return iterator{&_assigned, _assigned.end()}; }
	using const_iterator = Iter<decltype(_assigned)::const_iterator, std::add_const_t>;
	const_iterator begin() const {
		return const_iterator{&_assigned, _assigned.begin()}.skipEmpty();
	}
	const_iterator end() const { return const_iterator{&_assigned, _assigned.end()}; }

	// Assigns a new ProtoPalAttrs in a free slot, assuming there is one
	// Args are passed to the `ProtoPalAttrs`'s constructor
	template<typename... Ts>
	void assign(Ts &&...args) {
		auto freeSlot =
		    std::find_if_not(RANGE(_assigned), [](std::optional<ProtoPalAttrs> const &slot) {
			    return slot.has_value();
		    });

		if (freeSlot == _assigned.end()) { // We are full, use a new slot
			_assigned.emplace_back(std::forward<Ts>(args)...);
		} else { // Reuse a free slot
			freeSlot->emplace(std::forward<Ts>(args)...);
		}
	}
	void remove(iterator const &iter) {
		iter._iter->reset(); // This time, we want to access the `optional` itself
	}
	void clear() { _assigned.clear(); }

	bool empty() const {
		return std::find_if(
		           RANGE(_assigned),
		           [](std::optional<ProtoPalAttrs> const &slot) { return slot.has_value(); }
		       )
		       == _assigned.end();
	}
	size_t nbProtoPals() const { return std::distance(RANGE(*this)); }

private:
	template<typename Iter>
	static void addUniqueColors(
	    std::unordered_set<uint16_t> &colors,
	    Iter iter,
	    Iter const &end,
	    std::vector<ProtoPalette> const &protoPals
	) {
		for (; iter != end; ++iter) {
			ProtoPalette const &protoPal = protoPals[iter->protoPalIndex];
			colors.insert(RANGE(protoPal));
		}
	}
	// This function should stay private because it returns a reference to a unique object
	std::unordered_set<uint16_t> &uniqueColors() const {
		// We check for *distinct* colors by stuffing them into a `set`; this should be
		// faster than "back-checking" on every element (O(n²))
		static std::unordered_set<uint16_t> colors;

		colors.clear();
		addUniqueColors(colors, RANGE(*this), *_protoPals);
		return colors;
	}
public:
	// Returns the number of distinct colors
	size_t volume() const { return uniqueColors().size(); }
	bool canFit(ProtoPalette const &protoPal) const {
		auto &colors = uniqueColors();
		colors.insert(RANGE(protoPal));
		return colors.size() <= options.maxOpaqueColors();
	}

	// The `relSizeOf` method below should compute the sum, for each color in `protoPal`, of
	// the reciprocal of the "multiplicity" of the color across "our" proto-palettes.
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

	// Computes the "relative size" of a proto-palette on this palette;
	// it's a measure of how much this proto-palette would "cost" to introduce.
	uint32_t relSizeOf(ProtoPalette const &protoPal) const {
		// NOTE: this function must not call `uniqueColors`, or one of its callers will break!

		uint32_t relSize = 0;
		for (uint16_t color : protoPal) {
			auto multiplicity = // How many of our proto-palettes does this color also belong to?
			    std::count_if(RANGE(*this), [this, &color](ProtoPalAttrs const &attrs) {
				    ProtoPalette const &pal = (*_protoPals)[attrs.protoPalIndex];
				    return std::find(RANGE(pal), color) != pal.end();
			    });
			// We increase the denominator by 1 here; the reference code does this,
			// but the paper does not. Not adding 1 makes a multiplicity of 0 cause a division by 0
			// (that is, if the color is not found in any proto-palette), and adding 1 still seems
			// to preserve the paper's reasoning.
			//
			// The scale factor should ensure integer divisions only.
			assume(scaleFactor % (multiplicity + 1) == 0);
			relSize += scaleFactor / (multiplicity + 1);
		}
		return relSize;
	}

	// Computes the "relative size" of a set of proto-palettes on this palette
	template<typename Iter>
	auto combinedVolume(Iter &&begin, Iter const &end, std::vector<ProtoPalette> const &protoPals)
	    const {
		auto &colors = uniqueColors();
		addUniqueColors(colors, std::forward<Iter>(begin), end, protoPals);
		return colors.size();
	}
	// Computes the "relative size" of a set of colors on this palette
	template<typename Iter>
	auto combinedVolume(Iter &&begin, Iter &&end) const {
		auto &colors = uniqueColors();
		colors.insert(std::forward<Iter>(begin), std::forward<Iter>(end));
		return colors.size();
	}
};

static void decant(
    std::vector<AssignedProtos> &assignments, std::vector<ProtoPalette> const &protoPalettes
) {
	// "Decanting" is the process of moving all *things* that can fit in a lower index there
	auto decantOn = [&assignments](auto const &tryDecanting) {
		// No need to attempt decanting on palette #0, as there are no palettes to decant to
		for (size_t from = assignments.size(); --from;) {
			// Scan all palettes before this one
			for (size_t to = 0; to < from; ++to) {
				tryDecanting(assignments[to], assignments[from]);
			}

			// If the proto-palette is now empty, remove it
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

	options.verbosePrint(
	    Options::VERB_DEBUG, "%zu palettes before decanting\n", assignments.size()
	);

	// Decant on palettes
	decantOn([&protoPalettes](AssignedProtos &to, AssignedProtos &from) {
		// If the entire palettes can be merged, move all of `from`'s proto-palettes
		if (to.combinedVolume(RANGE(from), protoPalettes) <= options.maxOpaqueColors()) {
			for (ProtoPalAttrs &attrs : from) {
				to.assign(attrs.protoPalIndex);
			}
			from.clear();
		}
	});
	options.verbosePrint(
	    Options::VERB_DEBUG, "%zu palettes after decanting on palettes\n", assignments.size()
	);

	// Decant on "components" (= proto-pals sharing colors)
	decantOn([&protoPalettes](AssignedProtos &to, AssignedProtos &from) {
		// We need to iterate on all the "components", which are groups of proto-palettes sharing at
		// least one color with another proto-palettes in the group.
		// We do this by adding the first available proto-palette, and then looking for palettes
		// with common colors. (As an optimization, we know we can skip palettes already scanned.)
		std::vector<bool> processed(from.nbProtoPals(), false);
		std::unordered_set<uint16_t> colors;
		std::vector<size_t> members;
		while (true) {
			auto iter = std::find(RANGE(processed), true);
			if (iter == processed.end()) { // Processed everything!
				break;
			}
			auto attrs = from.begin();
			std::advance(attrs, iter - processed.begin());

			// Build up the "component"...
			colors.clear();
			members.clear();
			assume(members.empty()); // Compiler optimization hint
			do {
				ProtoPalette const &protoPal = protoPalettes[attrs->protoPalIndex];
				// If this is the first proto-pal, or if at least one color matches, add it
				if (members.empty()
				    || std::find_first_of(RANGE(colors), RANGE(protoPal)) != colors.end()) {
					colors.insert(RANGE(protoPal));
					members.push_back(iter - processed.begin());
					*iter = true; // Mark that proto-pal as processed
				}
				++iter;
				++attrs;
			} while (iter != processed.end());

			if (to.combinedVolume(RANGE(colors)) <= options.maxOpaqueColors()) {
				// Iterate through the component's proto-palettes, and transfer them
				auto member = from.begin();
				size_t curIndex = 0;
				for (size_t index : members) {
					std::advance(member, index - curIndex);
					curIndex = index;
					to.assign(std::move(*member));
					from.remove(member); // Removing does not shift elements, so it's cheap
				}
			}
		}
	});
	options.verbosePrint(
	    Options::VERB_DEBUG, "%zu palettes after decanting on \"components\"\n", assignments.size()
	);

	// Decant on individual proto-palettes
	decantOn([&protoPalettes](AssignedProtos &to, AssignedProtos &from) {
		for (auto iter = from.begin(); iter != from.end(); ++iter) {
			if (to.canFit(protoPalettes[iter->protoPalIndex])) {
				to.assign(std::move(*iter));
				from.remove(iter);
			}
		}
	});
	options.verbosePrint(
	    Options::VERB_DEBUG, "%zu palettes after decanting on proto-palettes\n", assignments.size()
	);
}

std::tuple<std::vector<size_t>, size_t>
    overloadAndRemove(std::vector<ProtoPalette> const &protoPalettes) {
	options.verbosePrint(
	    Options::VERB_LOG_ACT, "Paginating palettes using \"overload-and-remove\" strategy...\n"
	);

	// Sort the proto-palettes by size, which improves the packing algorithm's efficiency
	auto const indexOfLargestProtoPalFirst = [&protoPalettes](size_t left, size_t right) {
		ProtoPalette const &lhs = protoPalettes[left];
		ProtoPalette const &rhs = protoPalettes[right];
		return lhs.size() > rhs.size(); // We want the proto-pals to be sorted *largest first*!
	};
	std::vector<size_t> sortedProtoPalIDs;
	sortedProtoPalIDs.reserve(protoPalettes.size());
	for (size_t i = 0; i < protoPalettes.size(); ++i) {
		sortedProtoPalIDs.insert(
		    std::lower_bound(RANGE(sortedProtoPalIDs), i, indexOfLargestProtoPalFirst), i
		);
	}

	// Begin with all proto-palettes queued up for insertion
	std::queue<ProtoPalAttrs> queue(std::deque<ProtoPalAttrs>(RANGE(sortedProtoPalIDs)));
	// Begin with no pages
	std::vector<AssignedProtos> assignments{};

	for (; !queue.empty(); queue.pop()) {
		ProtoPalAttrs const &attrs = queue.front(); // Valid until the `queue.pop()`
		options.verbosePrint(
		    Options::VERB_TRACE, "Handling proto-palette %zu\n", attrs.protoPalIndex
		);

		ProtoPalette const &protoPal = protoPalettes[attrs.protoPalIndex];
		size_t bestPalIndex = assignments.size();
		// We're looking for a palette where the proto-palette's relative size is less than
		// its actual size; so only overwrite the "not found" index on meeting that criterion
		uint32_t bestRelSize = protoPal.size() * AssignedProtos::scaleFactor;

		for (size_t i = 0; i < assignments.size(); ++i) {
			// Skip the page if this one is banned from it
			if (attrs.isBannedFrom(i)) {
				continue;
			}

			uint32_t relSize = assignments[i].relSizeOf(protoPal);
			options.verbosePrint(
			    Options::VERB_TRACE,
			    "  Relative size to palette %zu (of %zu): %" PRIu32 " (size = %zu)\n",
			    i,
			    assignments.size(),
			    relSize,
			    protoPal.size()
			);
			if (relSize < bestRelSize) {
				bestPalIndex = i;
				bestRelSize = relSize;
			}
		}

		if (bestPalIndex == assignments.size()) {
			// Found nowhere to put it, create a new page containing just that one
			options.verbosePrint(
			    Options::VERB_TRACE,
			    "Assigning proto-palette %zu to new palette %zu\n",
			    attrs.protoPalIndex,
			    bestPalIndex
			);
			assignments.emplace_back(protoPalettes, std::move(attrs));
		} else {
			options.verbosePrint(
			    Options::VERB_TRACE,
			    "Assigning proto-palette %zu to palette %zu\n",
			    attrs.protoPalIndex,
			    bestPalIndex
			);
			auto &bestPal = assignments[bestPalIndex];
			// Add the color to that palette
			bestPal.assign(std::move(attrs));

			// If this overloads the palette, get it back to normal (if possible)
			while (bestPal.volume() > options.maxOpaqueColors()) {
				options.verbosePrint(
				    Options::VERB_TRACE,
				    "Palette %zu is overloaded! (%zu > %" PRIu8 ")\n",
				    bestPalIndex,
				    bestPal.volume(),
				    options.maxOpaqueColors()
				);

				// Look for a proto-pal minimizing "efficiency" (size / rel_size)
				auto [minEfficiencyIter, maxEfficiencyIter] = std::minmax_element(
				    RANGE(bestPal),
				    [&bestPal, &protoPalettes](ProtoPalAttrs const &lhs, ProtoPalAttrs const &rhs) {
					    ProtoPalette const &lhsProtoPal = protoPalettes[lhs.protoPalIndex];
					    ProtoPalette const &rhsProtoPal = protoPalettes[rhs.protoPalIndex];
					    size_t lhsSize = lhsProtoPal.size();
					    size_t rhsSize = rhsProtoPal.size();
					    uint32_t lhsRelSize = bestPal.relSizeOf(lhsProtoPal);
					    uint32_t rhsRelSize = bestPal.relSizeOf(rhsProtoPal);

					    options.verbosePrint(
					        Options::VERB_TRACE,
					        "  Proto-palettes %zu <=> %zu: Efficiency: %zu / %" PRIu32 " <=> %zu / "
					        "%" PRIu32 "\n",
					        lhs.protoPalIndex,
					        rhs.protoPalIndex,
					        lhsSize,
					        lhsRelSize,
					        rhsSize,
					        rhsRelSize
					    );
					    // This comparison is algebraically equivalent to
					    // `lhsSize / lhsRelSize < rhsSize / rhsRelSize`,
					    // but without potential precision loss from floating-point division.
					    return lhsSize * rhsRelSize < rhsSize * lhsRelSize;
				    }
				);

				// All efficiencies are identical iff min equals max
				ProtoPalette const &minProtoPal = protoPalettes[minEfficiencyIter->protoPalIndex];
				ProtoPalette const &maxProtoPal = protoPalettes[maxEfficiencyIter->protoPalIndex];
				size_t minSize = minProtoPal.size();
				size_t maxSize = maxProtoPal.size();
				uint32_t minRelSize = bestPal.relSizeOf(minProtoPal);
				uint32_t maxRelSize = bestPal.relSizeOf(maxProtoPal);
				options.verbosePrint(
				    Options::VERB_TRACE,
				    "  Proto-palettes %zu <= %zu: Efficiency: %zu / %" PRIu32 " <= %zu / %" PRIu32
				    "\n",
				    minEfficiencyIter->protoPalIndex,
				    maxEfficiencyIter->protoPalIndex,
				    minSize,
				    minRelSize,
				    maxSize,
				    maxRelSize
				);
				// This comparison is algebraically equivalent to
				// `maxSize / maxRelSize == minSize / minRelSize`,
				// but without potential precision loss from floating-point division.
				if (maxSize * minRelSize == minSize * maxRelSize) {
					options.verbosePrint(Options::VERB_TRACE, "  All efficiencies are identical\n");
					break;
				}

				// Remove the proto-pal with minimal efficiency
				options.verbosePrint(
				    Options::VERB_TRACE,
				    "  Removing proto-palette %zu\n",
				    minEfficiencyIter->protoPalIndex
				);
				queue.emplace(std::move(*minEfficiencyIter));
				queue.back().banFrom(bestPalIndex); // Ban it from this palette
				bestPal.remove(minEfficiencyIter);
			}
		}
	}

	// Deal with palettes still overloaded, by emptying them
	auto const &largestProtoPalFirst =
	    [&protoPalettes](ProtoPalAttrs const &lhs, ProtoPalAttrs const &rhs) {
		    return protoPalettes[lhs.protoPalIndex].size()
		           > protoPalettes[rhs.protoPalIndex].size();
	    };
	std::vector<ProtoPalAttrs> overloadQueue{};
	for (AssignedProtos &pal : assignments) {
		if (pal.volume() > options.maxOpaqueColors()) {
			for (ProtoPalAttrs &attrs : pal) {
				overloadQueue.emplace(
				    std::lower_bound(RANGE(overloadQueue), attrs, largestProtoPalFirst),
				    std::move(attrs)
				);
			}
			pal.clear();
		}
	}
	// Place back any proto-palettes now in the queue via first-fit
	for (ProtoPalAttrs const &attrs : overloadQueue) {
		ProtoPalette const &protoPal = protoPalettes[attrs.protoPalIndex];
		auto iter = std::find_if(RANGE(assignments), [&protoPal](AssignedProtos const &pal) {
			return pal.canFit(protoPal);
		});
		if (iter == assignments.end()) { // No such page, create a new one
			options.verbosePrint(
			    Options::VERB_DEBUG,
			    "Adding new palette (%zu) for overflowing proto-palette %zu\n",
			    assignments.size(),
			    attrs.protoPalIndex
			);
			assignments.emplace_back(protoPalettes, std::move(attrs));
		} else {
			options.verbosePrint(
			    Options::VERB_DEBUG,
			    "Assigning overflowing proto-palette %zu to palette %zu\n",
			    attrs.protoPalIndex,
			    iter - assignments.begin()
			);
			iter->assign(std::move(attrs));
		}
	}

	// LCOV_EXCL_START
	if (options.verbosity >= Options::VERB_INTERM) {
		for (auto &&assignment : assignments) {
			fputs("{ ", stderr);
			for (auto &&attrs : assignment) {
				fprintf(stderr, "[%zu] ", attrs.protoPalIndex);
				for (auto &&colorIndex : protoPalettes[attrs.protoPalIndex]) {
					fprintf(stderr, "%04" PRIx16 ", ", colorIndex);
				}
			}
			fprintf(stderr, "} (volume = %zu)\n", assignment.volume());
		}
	}
	// LCOV_EXCL_STOP

	// "Decant" the result
	decant(assignments, protoPalettes);
	// Note that the result does not contain any empty palettes

	// LCOV_EXCL_START
	if (options.verbosity >= Options::VERB_INTERM) {
		for (auto &&assignment : assignments) {
			fputs("{ ", stderr);
			for (auto &&attrs : assignment) {
				fprintf(stderr, "[%zu] ", attrs.protoPalIndex);
				for (auto &&colorIndex : protoPalettes[attrs.protoPalIndex]) {
					fprintf(stderr, "%04" PRIx16 ", ", colorIndex);
				}
			}
			fprintf(stderr, "} (volume = %zu)\n", assignment.volume());
		}
	}
	// LCOV_EXCL_STOP

	std::vector<size_t> mappings(protoPalettes.size());
	for (size_t i = 0; i < assignments.size(); ++i) {
		for (ProtoPalAttrs const &attrs : assignments[i]) {
			mappings[attrs.protoPalIndex] = i;
		}
	}
	return {mappings, assignments.size()};
}
