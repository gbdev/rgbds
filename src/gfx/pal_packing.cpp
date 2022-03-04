/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx/pal_packing.hpp"

#include <assert.h>
#include <bitset>
#include <inttypes.h>
#include <numeric>
#include <optional>
#include <queue>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "gfx/main.hpp"
#include "gfx/proto_palette.hpp"

using std::swap;

namespace packing {

// The solvers here are picked from the paper at http://arxiv.org/abs/1605.00558:
// "Algorithms for the Pagination Problem, a Bin Packing with Overlapping Items"
// Their formulation of the problem consists in packing "tiles" into "pages"; here is a
// correspondence table for our application of it:
// Paper | RGBGFX
// ------+-------
//  Tile | Proto-palette
//  Page | Palette

/**
 * A reference to a proto-palette, and attached attributes for sorting purposes
 */
struct ProtoPalAttrs {
	size_t const palIndex;
	/**
	 * Pages from which we are banned (to prevent infinite loops)
	 * This is dynamic because we wish not to hard-cap the amount of palettes
	 */
	std::vector<bool> bannedPages;

	ProtoPalAttrs(size_t index) : palIndex(index) {}
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

/**
 * A collection of proto-palettes assigned to a palette
 * Does not contain the actual color indices because we need to be able to remove elements
 */
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
		using iterator_category = std::input_iterator_tag;

	private:
		Constness<decltype(_assigned)> *_array = nullptr;
		Inner _iter{};

		Iter(decltype(_array) array, decltype(_iter) &&iter) : _array(array), _iter(iter) {
			skipEmpty();
		}
		void skipEmpty() {
			while (_iter != _array->end() && !_iter->has_value()) {
				++_iter;
			}
		}

	public:
		Iter() = default;

		bool operator==(Iter const &other) const { return _iter == other._iter; }
		bool operator!=(Iter const &other) const { return !(*this == other); }
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
			assert((*_iter).has_value());
			return **_iter;
		}
		pointer operator->() const {
			return &(**this); // Invokes the operator above, not quite a no-op!
		}

		friend void swap(Iter &lhs, Iter &rhs) {
			swap(lhs._array, rhs._array);
			swap(lhs._iter, rhs._iter);
		}
	};
public:
	using iterator = Iter<decltype(_assigned)::iterator, std::remove_const_t>;
	iterator begin() { return iterator{&_assigned, _assigned.begin()}; }
	iterator end() { return iterator{&_assigned, _assigned.end()}; }
	using const_iterator = Iter<decltype(_assigned)::const_iterator, std::add_const_t>;
	const_iterator begin() const { return const_iterator{&_assigned, _assigned.begin()}; }
	const_iterator end() const { return const_iterator{&_assigned, _assigned.end()}; }

	/**
	 * Assigns a new ProtoPalAttrs in a free slot, assuming there is one
	 * Args are passed to the `ProtoPalAttrs`'s constructor
	 */
	template<typename... Ts>
	void assign(Ts &&...args) {
		auto freeSlot = std::find_if_not(
			_assigned.begin(), _assigned.end(),
			[](std::optional<ProtoPalAttrs> const &slot) { return slot.has_value(); });

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

	bool empty() const { return std::distance(begin(), end()) == 0; }

private:
	static void addUniqueColors(std::unordered_set<uint16_t> &colors, AssignedProtos const &pal) {
		for (ProtoPalAttrs const &attrs : pal) {
			for (uint16_t color : (*pal._protoPals)[attrs.palIndex]) {
				colors.insert(color);
			}
		}
	}
	std::unordered_set<uint16_t> &uniqueColors() const {
		// We check for *distinct* colors by stuffing them into a `set`; this should be
		// faster than "back-checking" on every element (O(n²))
		//
		// TODO: calc84maniac suggested another approach; try implementing it, see if it
		// performs better:
		// > So basically you make a priority queue that takes iterators into each of your sets
		// > (paired with end iterators so you'll know where to stop), and the comparator tests the
		// > values pointed to by each iterator
		// > Then each iteration you pop from the queue,
		// > optionally add one to your count, increment the iterator and push it back into the
		// > queue if it didn't reach the end
		// > And you do this until the priority queue is empty
		static std::unordered_set<uint16_t> colors;

		colors.clear();
		addUniqueColors(colors, *this);
		return colors;
	}
public:
	/**
	 * Returns the number of distinct colors
	 */
	size_t volume() const { return uniqueColors().size(); }
	bool canFit(ProtoPalette const &protoPal) const {
		auto &colors = uniqueColors();
		colors.insert(protoPal.begin(), protoPal.end());
		return colors.size() <= options.maxPalSize();
	}

public:
	/**
	 * Computes the "relative size" of a proto-palette on this palette
	 */
	double relSizeOf(ProtoPalette const &protoPal) const {
		// NOTE: this function must not call `uniqueColors`, or one of its callers will break
		return std::transform_reduce(
			protoPal.begin(), protoPal.end(), 0.0, std::plus<>(), [this](uint16_t color) {
				// NOTE: The paper and the associated code disagree on this: the code has
			    // this `1 +`, whereas the paper does not; its lack causes a division by 0
			    // if the symbol is not found anywhere, so I'm assuming the paper is wrong.
				return 1.
			           / (1
			              + std::count_if(
							  begin(), end(), [this, &color](ProtoPalAttrs const &attrs) {
								  ProtoPalette const &pal = (*_protoPals)[attrs.palIndex];
								  return std::find(pal.begin(), pal.end(), color) != pal.end();
							  }));
			});
	}

	/**
	 * Computes the "relative size" of a palette on this one
	 */
	double combinedVolume(AssignedProtos const &pal) const {
		auto &colors = uniqueColors();
		addUniqueColors(colors, pal);
		return colors.size();
	}
};

static void removeEmptyPals(std::vector<AssignedProtos> &assignments) {
	// We do this by plucking "replacement" palettes from the end of the vector, so as to minimize
	// the amount of moves performed. We can afford this because we don't care about their order,
	// unlike `std::remove_if`, which permits less moves and thus better performance.
	for (size_t i = 0; i != assignments.size(); ++i) {
		if (assignments[i].empty()) {
			// Hinting the compiler that the `return;` can only be reached if entering the loop
			// produces better assembly
			if (assignments.back().empty()) {
				do {
					assignments.pop_back();
					assert(assignments.size() != 0);
				} while (assignments.back().empty());
				// Worst case, the loop ended on `assignments[i - 1]` (since every slot before `i`
				// is known to be non-empty).
				// (This could be a problem if `i` was 0, but we know there must be at least one
				// color, so we're safe from that. The assertion in the loop checks it to be sure.)
				// However, if it did stop at `i - 1`, then `i` no longer points to a valid slot,
				// and we must end.
				if (i == assignments.size()) {
					break;
				}
			}
			assert(i < assignments.size());
			assignments[i] = std::move(assignments.back());
			assignments.pop_back();
		}
	}
}

static void decant(std::vector<AssignedProtos> &assignments) {
	// "Decanting" is the process of moving all *things* that can fit in a lower index there
	auto decantOn = [&assignments](auto const &move) {
		// No need to attempt decanting on palette #0, as there are no palettes to decant to
		for (size_t from = assignments.size(); --from;) {
			// Scan all palettes before this one
			for (size_t to = 0; to < from; ++to) {
				move(assignments[to], assignments[from]);
			}
		}
	};

	// Decant on palettes
	decantOn([](AssignedProtos &to, AssignedProtos &from) {
		// If the entire palettes can be merged, move all of `from`'s proto-palettes
		if (to.combinedVolume(from) <= options.maxPalSize()) {
			for (ProtoPalAttrs &protoPal : from) {
				to.assign(std::move(protoPal));
			}
			from.clear();
		}
	});

	// Decant on "components" (= proto-pals sharing colors)
	decantOn([](AssignedProtos &to, AssignedProtos &from) {
		// TODO
		(void)to;
		(void)from;
	});

	// Decant on proto-palettes
	decantOn([](AssignedProtos &to, AssignedProtos &from) {
		// TODO
		(void)to;
		(void)from;
	});
}

std::tuple<DefaultInitVec<size_t>, size_t>
	overloadAndRemove(std::vector<ProtoPalette> const &protoPalettes) {
	options.verbosePrint("Paginating palettes using \"overload-and-remove\" strategy...\n");

	struct Iota {
		using value_type = size_t;
		using difference_type = size_t;
		using pointer = value_type const *;
		using reference = value_type const &;
		using iterator_category = std::input_iterator_tag;

		// Use aggregate init etc.
		value_type i;

		bool operator!=(Iota const &other) { return i != other.i; }
		reference operator*() const { return i; }
		pointer operator->() const { return &i; }
		Iota operator++() {
			++i;
			return *this;
		}
		Iota operator++(int) {
			Iota copy = *this;
			++i;
			return copy;
		}
	};

	// Begin with all proto-palettes queued up for insertion
	std::queue queue(std::deque<ProtoPalAttrs>(Iota{0}, Iota{protoPalettes.size()}));
	// Begin with no pages
	std::vector<AssignedProtos> assignments{};

	for (; !queue.empty(); queue.pop()) {
		ProtoPalAttrs const &attrs = queue.front(); // Valid until the `queue.pop()`

		ProtoPalette const &protoPal = protoPalettes[attrs.palIndex];
		size_t bestPalIndex = assignments.size();
		// We're looking for a palette where the proto-palette's relative size is less than
		// its actual size; so only overwrite the "not found" index on meeting that criterion
		double bestRelSize = protoPal.size();

		for (size_t i = 0; i < assignments.size(); ++i) {
			// Skip the page if this one is banned from it
			if (attrs.isBannedFrom(i)) {
				continue;
			}

			options.verbosePrint("%zu/%zu: Rel size: %f (size = %zu)\n", i, assignments.size(),
			                     assignments[i].relSizeOf(protoPal), protoPal.size());
			if (assignments[i].relSizeOf(protoPal) < bestRelSize) {
				bestPalIndex = i;
			}
		}

		if (bestPalIndex == assignments.size()) {
			// Found nowhere to put it, create a new page containing just that one
			assignments.emplace_back(protoPalettes, std::move(attrs));
		} else {
			auto &bestPal = assignments[bestPalIndex];
			// Add the color to that palette
			bestPal.assign(std::move(attrs));

			// If this overloads the palette, get it back to normal (if possible)
			while (bestPal.volume() > options.maxPalSize()) {
				options.verbosePrint("Palette %zu is overloaded! (%zu > %" PRIu8 ")\n",
				                     bestPalIndex, bestPal.volume(), options.maxPalSize());

				// Look for a proto-pal minimizing "efficiency" (size / rel_size)
				auto efficiency = [&bestPal](ProtoPalette const &pal) {
					return pal.size() / bestPal.relSizeOf(pal);
				};
				auto [minEfficiencyIter, maxEfficiencyIter] =
					std::minmax_element(bestPal.begin(), bestPal.end(),
				                        [&efficiency, &protoPalettes](ProtoPalAttrs const &lhs,
				                                                      ProtoPalAttrs const &rhs) {
											return efficiency(protoPalettes[lhs.palIndex])
					                               < efficiency(protoPalettes[rhs.palIndex]);
										});

				// All efficiencies are identical iff min equals max
				// TODO: maybe not ideal to re-compute these two?
				// TODO: yikes for float comparison! I *think* this threshold is OK?
				if (efficiency(protoPalettes[maxEfficiencyIter->palIndex])
				        - efficiency(protoPalettes[minEfficiencyIter->palIndex])
				    < .001) {
					break;
				}

				// Remove the proto-pal with minimal efficiency
				queue.emplace(std::move(*minEfficiencyIter));
				queue.back().banFrom(bestPalIndex); // Ban it from this palette
				bestPal.remove(minEfficiencyIter);
			}
		}
	}

	// Deal with palettes still overloaded, by emptying them
	for (AssignedProtos &pal : assignments) {
		if (pal.volume() > options.maxPalSize()) {
			for (ProtoPalAttrs &attrs : pal) {
				queue.emplace(std::move(attrs));
			}
			pal.clear();
		}
	}
	// Place back any proto-palettes now in the queue via first-fit
	while (!queue.empty()) {
		ProtoPalAttrs const &attrs = queue.front();
		ProtoPalette const &protoPal = protoPalettes[attrs.palIndex];
		auto iter =
			std::find_if(assignments.begin(), assignments.end(),
		                 [&protoPal](AssignedProtos const &pal) { return pal.canFit(protoPal); });
		if (iter == assignments.end()) { // No such page, create a new one
			options.verbosePrint("Adding new palette for overflow\n");
			assignments.emplace_back(protoPalettes, std::move(attrs));
		} else {
			options.verbosePrint("Assigning overflow to palette %zu\n", iter - assignments.begin());
			iter->assign(std::move(attrs));
		}
		queue.pop();
	}

	// "Decant" the result
	decant(assignments);

	// Remove all empty palettes, filling the gaps created.
	removeEmptyPals(assignments);

	if (options.beVerbose) {
		for (auto &&assignment : assignments) {
			options.verbosePrint("{ ");
			for (auto &&attrs : assignment) {
				for (auto &&colorIndex : protoPalettes[attrs.palIndex]) {
					options.verbosePrint("%04" PRIx16 ", ", colorIndex);
				}
			}
			options.verbosePrint("} (volume = %zu)\n", assignment.volume());
		}
	}

	DefaultInitVec<size_t> mappings(protoPalettes.size());
	for (size_t i = 0; i < assignments.size(); ++i) {
		for (ProtoPalAttrs const &attrs : assignments[i]) {
			mappings[attrs.palIndex] = i;
		}
	}
	return {mappings, assignments.size()};
}

} // namespace packing
