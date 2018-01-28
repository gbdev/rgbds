/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2017-2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "version.h"

const char *get_package_version_string(void)
{
	static char s[50];

	/* The following conditional should be simplified by the compiler. */
	if (strlen(BUILD_VERSION_STRING) == 0) {
		snprintf(s, sizeof(s), "v%d.%d.%d", PACKAGE_VERSION_MAJOR,
			 PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCH);
		return s;
	} else {
		return BUILD_VERSION_STRING;
	}
}
