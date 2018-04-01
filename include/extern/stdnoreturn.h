/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2014-2018, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef EXTERN_STDNORETURN_H
#define EXTERN_STDNORETURN_H

#if defined(__STDC_VERSION__)
	#if __STDC_VERSION__ >= 201112L
		/* C11 or newer */
		#define noreturn _Noreturn
	#endif
#endif

#if defined(__GNUC__) && !defined(noreturn)
	#if __GNUC__ > 2 || (__GNUC__ == 2 && (__GNUC_MINOR__ >= 5))
		/* GCC 2.5 or newer */
		#define noreturn __attribute__ ((noreturn))
	#endif
#endif

#if defined(_MSC_VER) && !defined(noreturn)
	#if _MSC_VER >= 1310
		/* MS Visual Studio 2003/.NET Framework 1.1 or newer */
		#define noreturn _declspec(noreturn)
	#endif
#endif

#if !defined(noreturn)
	/* Unsupported, but no need to throw a fit */
	#define noreturn
#endif

#endif /* EXTERN_STDNORETURN_H */
