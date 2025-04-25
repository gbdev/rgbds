// SPDX-License-Identifier: MIT

#ifndef RGBDS_ITERTOOLS_HPP
#define RGBDS_ITERTOOLS_HPP

#include <tuple>
#include <utility>

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

		auto operator*() const { return _value; }

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
