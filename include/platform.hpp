/* SPDX-License-Identifier: MIT */

// platform-specific hacks

#ifndef RGBDS_PLATFORM_HPP
#define RGBDS_PLATFORM_HPP

// MSVC doesn't have str(n)casecmp, use a suitable replacement
#ifdef _MSC_VER
	#include <string.h> // IWYU pragma: export
	#define strcasecmp  _stricmp
	#define strncasecmp _strnicmp
#else
	#include <strings.h> // IWYU pragma: export
#endif

// MSVC prefixes the names of S_* macros with underscores,
// and doesn't define any S_IS* macros; define them ourselves
#ifdef _MSC_VER
	#define S_IFMT        _S_IFMT
	#define S_IFDIR       _S_IFDIR
	#define S_ISDIR(mode) (((mode) & (S_IFMT)) == S_IFDIR)
#endif

// MSVC doesn't use POSIX types or defines for `read`
#ifdef _MSC_VER
	#include <io.h> // IWYU pragma: export
	#define STDIN_FILENO  0
	#define STDOUT_FILENO 1
	#define STDERR_FILENO 2
	#define ssize_t       int
	#define SSIZE_MAX     INT_MAX
#else
	#include <fcntl.h>  // IWYU pragma: export
	#include <limits.h> // IWYU pragma: export
	#include <unistd.h> // IWYU pragma: export
#endif

// MSVC uses a different name for O_RDWR, and needs an additional _O_BINARY flag
#ifdef _MSC_VER
	#include <fcntl.h> // IWYU pragma: export
	#define O_RDWR         _O_RDWR
	#define S_ISREG(field) ((field) & (_S_IFREG))
	#define O_BINARY       _O_BINARY
	#define O_TEXT         _O_TEXT
#elif !defined(O_BINARY) // Cross-compilers define O_BINARY
	#define O_BINARY 0   // POSIX says we shouldn't care!
	#define O_TEXT   0   // Assume that it's not defined either
#endif                   // _MSC_VER

// Windows has stdin and stdout open as text by default, which we may not want
#if defined(_MSC_VER) || defined(__MINGW32__)
	#include <io.h> // IWYU pragma: export
	#define setmode(fd, mode) _setmode(fd, mode)
#else
	#define setmode(fd, mode) (0)
#endif

#endif // RGBDS_PLATFORM_HPP
