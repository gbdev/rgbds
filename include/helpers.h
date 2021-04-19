/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2014-2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

#include "platform.h"

// Of course, MSVC does not support C11, so no _Noreturn there...
#ifdef _MSC_VER
	#define _Noreturn __declspec(noreturn)
#endif

// Ideally, we'd use `__has_attribute` and `__has_builtin`, but these were only introduced in GCC 9
#ifdef __GNUC__ // GCC or compatible
	#define format_(archetype, str_index, first_arg) \
		__attribute__ ((format (archetype, str_index, first_arg)))
	// In release builds, define "unreachable" as such, but trap in debug builds
	#ifdef NDEBUG
		#define unreachable_	__builtin_unreachable
	#else
		#define unreachable_	__builtin_trap
	#endif
#else
	// Unsupported, but no need to throw a fit
	#define format_(archetype, str_index, first_arg)
	// This seems to generate similar code to __builtin_unreachable, despite different semantics
	// Note that executing this is undefined behavior (declared _Noreturn, but does return)
	static inline _Noreturn unreachable_(void) {}
#endif

// Use builtins whenever possible, and shim them otherwise
#ifdef __GNUC__ // GCC or compatible
	#define ctz __builtin_ctz
	#define clz __builtin_clz

#elif defined(_MSC_VER)
	#include <assert.h>
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse, _BitScanForward)
	static inline int ctz(unsigned int x)
	{
		unsigned long cnt;

		assert(x != 0);
		_BitScanForward(&cnt, x);
		return cnt;
	}
	static inline int clz(unsigned int x)
	{
		unsigned long cnt;

		assert(x != 0);
		_BitScanReverse(&cnt, x);
		return 31 - cnt;
	}

#else
	// FIXME: these are rarely used, and need testing...
	#include <limits.h>
	static inline int ctz(unsigned int x)
	{
		int cnt = 0;

		while (!(x & 1)) {
			x >>= 1;
			cnt++;
		}
		return cnt;
	}

	static inline int clz(unsigned int x)
	{
		int cnt = 0;

		while (x <= UINT_MAX / 2) {
			x <<= 1;
			cnt++;
		}
		return cnt;
	}
#endif

// Macros for stringification
#define STR(x) #x
#define EXPAND_AND_STR(x) STR(x)

// Helper function to write a LE 32-bit value to a buffer

static inline uint32_t readLE32(uint8_t const ptr[MIN_NB_ELMS(4)])
{
	return ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
}

static inline void writeLE32(uint8_t ptr[MIN_NB_ELMS(4)], uint32_t i)
{
	ptr[0] = i;
	ptr[1] = i >> 8;
	ptr[2] = i >> 16;
	ptr[3] = i >> 24;
}

#endif /* HELPERS_H */
