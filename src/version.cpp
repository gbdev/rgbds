// SPDX-License-Identifier: MIT

#include "version.hpp"

#include "helpers.hpp"

#ifdef __clang__
	#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
		#define __SANITIZE_ADDRESS__
	#endif
	#if __has_feature(address_sanitizer) && !defined(__SANITIZE_UNDEFINED__)
		#define __SANITIZE_UNDEFINED__
	#endif
#endif

#if !defined(NDEBUG) && defined(__SANITIZE_ADDRESS__)
extern "C" {
	char const *__asan_default_options(void) {
		return ":check_initialization_order=1"
		       ":detect_invalid_pointer_pairs=2"
	// `detect_leaks` is not supported on macOS.
	#ifndef __APPLE__
		       ":detect_leaks=1"
	#endif
		       ":detect_stack_use_after_return=1"
		       // ":fast_unwind_on_malloc=0" // Enable this if ASan outputs bad backtraces
		       ":print_legend=0"
		       ":strict_init_order=1"
		       ":strict_string_checks=1";
	}
}
#endif

#if !defined(NDEBUG) && defined(__SANITIZE_UNDEFINED__)
extern "C" {
	char const *__ubsan_default_options(void) {
		return "print_stacktrace=1";
	}
}
#endif

char const *get_package_version_string() {
	if constexpr (literal_strlen(BUILD_VERSION_STRING) > 0) {
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
