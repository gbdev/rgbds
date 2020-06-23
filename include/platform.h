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

#endif /* RGBDS_PLATFORM_H */
