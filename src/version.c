/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2017-2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "helpers.h"
#include "version.h"

const char *get_package_version_string(void)
{
	// The following conditional should be simplified by the compiler.
	if (strlen(BUILD_VERSION_STRING) == 0) {
		// Fallback if version string can't be obtained from Git
#ifndef PACKAGE_VERSION_RC
		return "v" EXPAND_AND_STR(PACKAGE_VERSION_MAJOR)
			"." EXPAND_AND_STR(PACKAGE_VERSION_MINOR)
			"." EXPAND_AND_STR(PACKAGE_VERSION_PATCH);
#else
		return "v" EXPAND_AND_STR(PACKAGE_VERSION_MAJOR)
			"." EXPAND_AND_STR(PACKAGE_VERSION_MINOR)
			"." EXPAND_AND_STR(PACKAGE_VERSION_PATCH)
			"-rc" EXPAND_AND_STR(PACKAGE_VERSION_RC);
#endif
	} else {
		return BUILD_VERSION_STRING;
	}
}
