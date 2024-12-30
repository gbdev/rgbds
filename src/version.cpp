/* SPDX-License-Identifier: MIT */

#include "version.hpp"

#include <string.h>

#include "helpers.hpp"

// We do not build `make develop` with `-fsanitize=leak` because macOS clang++ does not support it.
// Instead, we enable ASan (`-fsanitize=address`) to check for memory leaks in all four programs.
#ifdef __clang__
	#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
		#define __SANITIZE_ADDRESS__
	#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(__APPLE__)
extern "C" {
	char const *__asan_default_options(void) {
		return "detect_leaks=1";
	}
}
#endif

// This variable is passed via `-D` from the Makefile, but not from CMake
// (in which `configure_file()` is used on this file to replace some syntax)
#ifndef BUILD_VERSION_STRING
    // CMake-specific syntax here
	#define BUILD_VERSION_STRING "@GIT_REV@"
#endif

char const *get_package_version_string() {
	if constexpr (QUOTEDSTRLEN(BUILD_VERSION_STRING) > 0) {
		return BUILD_VERSION_STRING;
	}
	// Fallback if version string can't be obtained from Git
#ifndef PACKAGE_VERSION_RC
	return "v" EXPAND_AND_STR(PACKAGE_VERSION_MAJOR) "." EXPAND_AND_STR(PACKAGE_VERSION_MINOR
	) "." EXPAND_AND_STR(PACKAGE_VERSION_PATCH);
#else
	return "v" EXPAND_AND_STR(PACKAGE_VERSION_MAJOR) "." EXPAND_AND_STR(PACKAGE_VERSION_MINOR
	) "." EXPAND_AND_STR(PACKAGE_VERSION_PATCH) "-rc" EXPAND_AND_STR(PACKAGE_VERSION_RC);
#endif
}
