/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020 RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* platform-specific hacks */

#ifndef RGBDS_PLATFORM_H
#define RGBDS_PLATFORM_H

/* MSVC doesn't have strncasecmp, use a suitable replacement */
#ifdef _MSC_VER
# include <string.h>
# define strncasecmp _strnicmp
#else
# include <strings.h>
#endif

/* MSVC has deprecated strdup in favor of _strdup */
#ifdef _MSC_VER
# define strdup _strdup
#endif

/* MSVC prefixes the names of S_* macros with underscores,
   and doesn't define any S_IS* macros. Define them ourselves */
#ifdef _MSC_VER
# define S_IFMT _S_IFMT
# define S_IFDIR _S_IFDIR
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

/* MSVC doesn't use POSIX types or defines for `read` */
#ifdef _MSC_VER
# define STDIN_FILENO 0
# define ssize_t int
# define SSIZE_MAX INT_MAX
#endif

#endif /* RGBDS_PLATFORM_H */
