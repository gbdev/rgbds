// SPDX-License-Identifier: MIT

#ifndef RGBDS_ITERTOOLS_HPP
#define RGBDS_ITERTOOLS_HPP

#include <deque>
#include <optional>
#include <stddef.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "helpers.hpp" // Enum, assume

// A wrapper around iterables to reverse their iteration order; used in `for`-each loops.
template<typename IterableT>
struct ReversedIterable {
	IterableT &_iterable;
};

template<typename IterableT>
auto begin(ReversedIterable<IterableT> r) {
	return std::rbegin(r._iterable);
}

template<typename IterableT>
auto end(ReversedIterable<IterableT> r) {
	return std::rend(r._iterable);
}

template<typename IterableT>
ReversedIterable<IterableT> reversed(IterableT &&_iterable) {
	return {_iterable};
}

// A map from `KeyT` keys to `ItemT` items, iterable in the order the items were inserted.
template<typename KeyT, typename ItemT>
class InsertionOrderedMap {
	std::deque<ItemT> list;               // `deque` does not invalidate item references
	std::unordered_map<KeyT, size_t> map; // Indexes into `list`

public:
	size_t size() const { return list.size(); }

	bool empty() const { return list.empty(); }

	bool contains(KeyT const &key) const { return map.find(key) != map.end(); }

	ItemT &operator[](size_t i) { return list[i]; }

	typename decltype(list)::iterator begin() { return list.begin(); }
	typename decltype(list)::iterator end() { return list.end(); }
	typename decltype(list)::const_iterator begin() const { return list.begin(); }
	typename decltype(list)::const_iterator end() const { return list.end(); }

	// Adding a key that already exists would make the previous value unreachable.
	// `InsertionOrderedMap`s are only used for charmaps and sections, which each
	// avoid `add`ing if already present, so we can `assume` this is not a concern.

	ItemT &add(KeyT const &key) {
		assume(!contains(key));
		map[key] = list.size();
		return list.emplace_back();
	}

	ItemT &add(KeyT const &key, ItemT &&value) {
		assume(!contains(key));
		map[key] = list.size();
		list.emplace_back(std::move(value));
		return list.back();
	}

	ItemT &addAnonymous() {
		// Add the new item to the list, but do not update the map
		return list.emplace_back();
	}

	std::optional<size_t> findIndex(KeyT const &key) const {
		if (auto search = map.find(key); search != map.end()) {
			return search->second;
		}
		return std::nullopt;
	}
};

// An iterable of `enum` values in the half-open range [start, stop).
template<Enum EnumT>
class EnumSeq {
	EnumT _start;
	EnumT _stop;

	class Iterator {
		EnumT _value;

	public:
		explicit Iterator(EnumT value) : _value(value) {}

		Iterator &operator++() {
			_value = static_cast<EnumT>(_value + 1);
			return *this;
		}

		EnumT operator*() const { return _value; }

		bool operator==(Iterator const &rhs) const { return _value == rhs._value; }
	};

public:
	explicit EnumSeq(EnumT stop) : _start(static_cast<EnumT>(0)), _stop(stop) {}
	explicit EnumSeq(EnumT start, EnumT stop) : _start(start), _stop(stop) {}

	Iterator begin() { return Iterator(_start); }
	Iterator end() { return Iterator(_stop); }
};

// Only needed inside `ZipContainer` below.
// This is not a fully generic implementation; its current use cases only require for-loop behavior.
// We also assume that all iterators have the same length.
template<typename... IteratorTs>
class ZipIterator {
	std::tuple<IteratorTs...> _iters;

public:
	explicit ZipIterator(std::tuple<IteratorTs...> &&iters) : _iters(iters) {}

	ZipIterator &operator++() {
		std::apply([](auto &&...it) { (++it, ...); }, _iters);
		return *this;
	}

	auto operator*() const {
		return std::apply(
		    [](auto &&...it) { return std::tuple<decltype(*it)...>(*it...); }, _iters
		);
	}

	bool operator==(ZipIterator const &rhs) const {
		return std::get<0>(_iters) == std::get<0>(rhs._iters);
	}
};

// Only needed inside `zip` below.
template<typename... IterableTs>
class ZipContainer {
	std::tuple<IterableTs...> _containers;

public:
	explicit ZipContainer(IterableTs &&...containers)
	    : _containers(std::forward<IterableTs>(containers)...) {}

	auto begin() {
		return ZipIterator(std::apply(
		    [](auto &&...containers) {
			    using std::begin;
			    return std::make_tuple(begin(containers)...);
		    },
		    _containers
		));
	}

	auto end() {
		return ZipIterator(std::apply(
		    [](auto &&...containers) {
			    using std::end;
			    return std::make_tuple(end(containers)...);
		    },
		    _containers
		));
	}
};

// Only needed inside `zip` below.
// Take ownership of objects and rvalue refs passed to us, but not lvalue refs
template<typename IterableT>
using ZipHolder = std::conditional_t<
    std::is_lvalue_reference_v<IterableT>,
    IterableT,
    std::remove_cvref_t<IterableT>>;

// Iterates over N containers at once, yielding tuples of N items at a time.
// Does the same number of iterations as the first container's iterator!
template<typename... IterableTs>
static constexpr auto zip(IterableTs &&...containers) {
	return ZipContainer<ZipHolder<IterableTs>...>(std::forward<IterableTs>(containers)...);
}

#endif // RGBDS_ITERTOOLS_HPP
