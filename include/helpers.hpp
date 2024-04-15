/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_HELPERS_HPP
#define RGBDS_HELPERS_HPP

// Ideally we'd use `std::unreachable`, but it has insufficient compiler support
#ifdef __GNUC__ // GCC or compatible
	#ifdef NDEBUG
		#define unreachable_ __builtin_unreachable
	#else
		// In release builds, define "unreachable" as such, but trap in debug builds
		#define unreachable_ __builtin_trap
	#endif
#else
// This seems to generate similar code to __builtin_unreachable, despite different semantics
// Note that executing this is undefined behavior (declared [[noreturn]], but does return)
[[noreturn]] static inline void unreachable_() {
}
#endif

// Ideally we'd use `[[assume()]]`, but it has insufficient compiler support
#ifdef NDEBUG
	#ifdef _MSC_VER
		#define assume(x) __assume(x)
	#else
		//  `[[gnu::assume()]]` for GCC or compatible also has insufficient support (GCC 13+ only)
		#define assume(x) \
			do { \
				if (!(x)) \
					unreachable_(); \
			} while (0)
	#endif
#else
	// In release builds, define "assume" as such, but `assert` in debug builds
	#include <assert.h>
	#define assume assert
#endif

// Ideally we'd use `std::bit_width`, but it has insufficient compiler support
#ifdef __GNUC__ // GCC or compatible
	#define ctz __builtin_ctz
	#define clz __builtin_clz

#elif defined(_MSC_VER)
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse, _BitScanForward)

static inline int ctz(unsigned int x) {
	unsigned long cnt;

	assume(x != 0);
	_BitScanForward(&cnt, x);
	return cnt;
}

static inline int clz(unsigned int x) {
	unsigned long cnt;

	assume(x != 0);
	_BitScanReverse(&cnt, x);
	return 31 - cnt;
}

#else
	#include <limits.h>

static inline int ctz(unsigned int x) {
	int cnt = 0;

	while (!(x & 1)) {
		x >>= 1;
		cnt++;
	}
	return cnt;
}

static inline int clz(unsigned int x) {
	int cnt = 0;

	while (x <= UINT_MAX / 2) {
		x <<= 1;
		cnt++;
	}
	return cnt;
}
#endif

// Macros for stringification
#define STR(x)            #x
#define EXPAND_AND_STR(x) STR(x)

// Macros for concatenation
#define CAT(x, y)            x##y
#define EXPAND_AND_CAT(x, y) CAT(x, y)

// Obtaining the size of an array; `arr` must be an expression, not a type!
// (Having two instances of `arr` is OK because the contents of `sizeof` are not evaluated.)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof *(arr))

// For lack of <ranges>, this adds some more brevity
#define RANGE(s) std::begin(s), std::end(s)

// MSVC does not inline `strlen()` or `.length()` of a constant string, so we use `sizeof`
#define QUOTEDSTRLEN(s) (sizeof(s) - 1)

// For ad-hoc RAII in place of a `defer` statement or cross-platform `__attribute__((cleanup))`
template<typename T>
struct Defer {
	T deferred;
	Defer(T func) : deferred(func) {}
	~Defer() { deferred(); }
};

#endif // RGBDS_HELPERS_HPP
