/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2014-2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HELPERS_H
#define HELPERS_H

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

// Macros for stringification
#define STR(x) #x
#define EXPAND_AND_STR(x) STR(x)

#endif /* HELPERS_H */
