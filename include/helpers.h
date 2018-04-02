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
	#define noreturn __attribute__ ((noreturn))
#else
	/* Unsupported, but no need to throw a fit */
	#define noreturn
#endif

#endif /* HELPERS_H */
