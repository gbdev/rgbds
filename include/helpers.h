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
	#define noreturn_	__attribute__ ((noreturn))
	#define unused_		__attribute__ ((unused))
#else
	/* Unsupported, but no need to throw a fit */
	#define noreturn_
	#define unused_
#endif

#endif /* HELPERS_H */
