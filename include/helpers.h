/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2014-2018, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HELPERS_H
#define HELPERS_H

#ifdef __GNUC__
	/* GCC or compatible */
	#define format_(archetype, str_index, first_arg) \
		__attribute__ ((format (archetype, str_index, first_arg)))
	#define noreturn_	__attribute__ ((noreturn))
	#define trap_		__builtin_trap()
#else
	/* Unsupported, but no need to throw a fit */
	#define format_(archetype, str_index, first_arg)
	#define noreturn_
	#define unused_
	#define trap_
#endif

#ifndef DEVELOP
	#define DEVELOP 0
#endif

/* Macros for stringification */
#define STR(x) #x
#define EXPAND_AND_STR(x) STR(x)

#endif /* HELPERS_H */
