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

template<typename T>
struct ReversedIterable {
	T &_iterable;
};

template<typename T>
auto begin(ReversedIterable<T> r) {
	return std::rbegin(r._iterable);
}

template<typename T>
auto end(ReversedIterable<T> r) {
	return std::rend(r._iterable);
}

template<typename T>
ReversedIterable<T> reversed(T &&_iterable) {
	return {_iterable};
}

template<typename T>
class InsertionOrderedMap {
	std::deque<T> list;
	std::unordered_map<std::string, size_t> map; // Indexes into `list`

public:
	size_t size() const { return list.size(); }

	bool empty() const { return list.empty(); }

	bool contains(std::string const &name) const { return map.find(name) != map.end(); }

	T &operator[](size_t i) { return list[i]; }

	typename decltype(list)::iterator begin() { return list.begin(); }
	typename decltype(list)::iterator end() { return list.end(); }
	typename decltype(list)::const_iterator begin() const { return list.begin(); }
	typename decltype(list)::const_iterator end() const { return list.end(); }

	T &add(std::string const &name) {
		map[name] = list.size();
		return list.emplace_back();
	}

	T &add(std::string const &name, T &&value) {
		map[name] = list.size();
		list.emplace_back(std::move(value));
		return list.back();
	}

	T &addAnonymous() {
		// Add the new item to the list, but do not update the map
		return list.emplace_back();
	}

	std::optional<size_t> findIndex(std::string const &name) const {
		if (auto search = map.find(name); search != map.end()) {
			return search->second;
		}
		return std::nullopt;
	}
};

template<typename T>
class EnumSeq {
	T _start;
	T _stop;

	class Iterator {
		T _value;

	public:
		explicit Iterator(T value) : _value(value) {}

		Iterator &operator++() {
			_value = static_cast<T>(_value + 1);
			return *this;
		}

		T operator*() const { return _value; }

		bool operator==(Iterator const &rhs) const { return _value == rhs._value; }
	};

public:
	explicit EnumSeq(T stop) : _start(static_cast<T>(0)), _stop(stop) {}
	explicit EnumSeq(T start, T stop) : _start(start), _stop(stop) {}

	Iterator begin() { return Iterator(_start); }
	Iterator end() { return Iterator(_stop); }
};

// This is not a fully generic implementation; its current use cases only require for-loop behavior.
// We also assume that all iterators have the same length.
template<typename... Ts>
class ZipIterator {
	std::tuple<Ts...> _iters;

public:
	explicit ZipIterator(std::tuple<Ts...> &&iters) : _iters(iters) {}

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

template<typename... Ts>
class ZipContainer {
	std::tuple<Ts...> _containers;

public:
	explicit ZipContainer(Ts &&...containers) : _containers(std::forward<Ts>(containers)...) {}

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

// Take ownership of objects and rvalue refs passed to us, but not lvalue refs
template<typename T>
using Holder = std::
    conditional_t<std::is_lvalue_reference_v<T>, T, std::remove_cv_t<std::remove_reference_t<T>>>;

// Does the same number of iterations as the first container's iterator!
template<typename... Ts>
static constexpr auto zip(Ts &&...cs) {
	return ZipContainer<Holder<Ts>...>(std::forward<Ts>(cs)...);
}

#endif // RGBDS_ITERTOOLS_HPP
