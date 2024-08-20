/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_EITHER_HPP
#define RGBDS_EITHER_HPP

#include <type_traits>
#include <utility>

#include "helpers.hpp" // assume

template<typename T1, typename T2>
union Either {
	typedef T1 type1;
	typedef T2 type2;

private:
	template<typename T, unsigned V>
	struct Field {
		constexpr static unsigned tag_value = V;

		unsigned tag = tag_value;
		T value;

		Field() : value() {}
		Field(T &value_) : value(value_) {}
		Field(T const &value_) : value(value_) {}
		Field(T &&value_) : value(std::move(value_)) {}
	};

	// The `_tag` unifies with the first `tag` member of each `struct`.
	constexpr static unsigned nulltag = 0;
	unsigned _tag = nulltag;
	Field<T1, 1> _t1;
	Field<T2, 2> _t2;

	// Value accessors; the function parameters are dummies for overload resolution.
	// Only used to implement `field()` below.
	auto &pick(T1 *) { return _t1; }
	auto const &pick(T1 *) const { return _t1; }
	auto &pick(T2 *) { return _t2; }
	auto const &pick(T2 *) const { return _t2; }

	// Generic field accessors; for internal use only.
	template<typename T>
	auto &field() {
		return pick((T *)nullptr);
	}
	template<typename T>
	auto const &field() const {
		return pick((T *)nullptr);
	}

public:
	// Equivalent of `std::monostate` for `std::variant`s.
	Either() : _tag() {}
	// These constructors cannot be generic over the value type, because that would prevent
	// constructible values from being inferred, e.g. a `const char *` string literal for an
	// `std::string` field value.
	Either(T1 &value) : _t1(value) {}
	Either(T2 &value) : _t2(value) {}
	Either(T1 const &value) : _t1(value) {}
	Either(T2 const &value) : _t2(value) {}
	Either(T1 &&value) : _t1(std::move(value)) {}
	Either(T2 &&value) : _t2(std::move(value)) {}

	// Destructor manually calls the appropriate value destructor.
	~Either() {
		if (_tag == _t1.tag_value) {
			_t1.value.~T1();
		} else if (_tag == _t2.tag_value) {
			_t2.value.~T2();
		}
	}

	// Copy assignment operators for each possible value.
	Either &operator=(T1 const &value) {
		_t1.tag = _t1.tag_value;
		new (&_t1.value) T1(value);
		return *this;
	}
	Either &operator=(T2 const &value) {
		_t2.tag = _t2.tag_value;
		new (&_t2.value) T2(value);
		return *this;
	}

	// Move assignment operators for each possible value.
	Either &operator=(T1 &&value) {
		_t1.tag = _t1.tag_value;
		new (&_t1.value) T1(std::move(value));
		return *this;
	}
	Either &operator=(T2 &&value) {
		_t2.tag = _t2.tag_value;
		new (&_t2.value) T2(std::move(value));
		return *this;
	}

	// Copy assignment operator from another `Either`.
	Either &operator=(Either other) {
		if (other._tag == other._t1.tag_value) {
			*this = other._t1.value;
		} else if (other._tag == other._t2.tag_value) {
			*this = other._t2.value;
		} else {
			_tag = nulltag;
		}
		return *this;
	}

	// Copy constructor from another `Either`; implemented in terms of value assignment operators.
	Either(Either const &other) {
		if (other._tag == other._t1.tag_value) {
			*this = other._t1.value;
		} else if (other._tag == other._t2.tag_value) {
			*this = other._t2.value;
		} else {
			_tag = nulltag;
		}
	}

	// Move constructor from another `Either`; implemented in terms of value assignment operators.
	Either(Either &&other) {
		if (other._tag == other._t1.tag_value) {
			*this = std::move(other._t1.value);
		} else if (other._tag == other._t2.tag_value) {
			*this = std::move(other._t2.value);
		} else {
			_tag = nulltag;
		}
	}

	// Equivalent of `.emplace<T>()` for `std::variant`s.
	template<typename T, typename... Args>
	void emplace(Args &&...args) {
		this->~Either();
		if constexpr (std::is_same_v<T, T1>) {
			_t1.tag = _t1.tag_value;
			new (&_t1.value) T1(std::forward<Args>(args)...);
		} else if constexpr (std::is_same_v<T, T2>) {
			_t2.tag = _t2.tag_value;
			new (&_t2.value) T2(std::forward<Args>(args)...);
		} else {
			_tag = nulltag;
		}
	}

	// Equivalent of `std::holds_alternative<std::monostate>()` for `std::variant`s.
	bool empty() const { return _tag == nulltag; }

	// Equivalent of `std::holds_alternative<T>()` for `std::variant`s.
	template<typename T>
	bool holds() const {
		if constexpr (std::is_same_v<T, T1>) {
			return _tag == _t1.tag_value;
		} else if constexpr (std::is_same_v<T, T2>) {
			return _tag == _t2.tag_value;
		} else {
			return false;
		}
	}

	// Equivalent of `std::get<T>()` for `std::variant`s.
	template<typename T>
	auto &get() {
		assume(holds<T>());
		return field<T>().value;
	}
	template<typename T>
	auto const &get() const {
		assume(holds<T>());
		return field<T>().value;
	}
};

#endif // RGBDS_EITHER_HPP
