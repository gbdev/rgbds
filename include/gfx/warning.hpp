// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_WARNING_HPP
#define RGBDS_GFX_WARNING_HPP

// Prints the error count, and exits with failure
[[noreturn]]
void giveUp();

// If any error has been emitted thus far, calls `giveUp()`
void requireZeroErrors();

// Prints an error, and increments the error count
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);

// Prints a fatal error, increments the error count, and gives up
[[gnu::format(printf, 1, 2), noreturn]]
void fatal(char const *fmt, ...);

#endif // RGBDS_GFX_WARNING_HPP
