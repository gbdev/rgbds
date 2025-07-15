// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_WARNING_HPP
#define RGBDS_LINK_WARNING_HPP

#include <stdint.h>

#define warningAt(where, ...) warning(where.src, where.lineNo, __VA_ARGS__)
#define errorAt(where, ...)   error(where.src, where.lineNo, __VA_ARGS__)
#define fatalAt(where, ...)   fatal(where.src, where.lineNo, __VA_ARGS__)

struct FileStackNode;

[[gnu::format(printf, 3, 4)]]
void warning(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void warning(char const *fmt, ...);

[[gnu::format(printf, 3, 4)]]
void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void errorNoDump(char const *fmt, ...);
[[gnu::format(printf, 2, 3)]]
void argErr(char flag, char const *fmt, ...);

[[gnu::format(printf, 3, 4), noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2), noreturn]]
void fatal(char const *fmt, ...);

void requireZeroErrors();

#endif // RGBDS_LINK_WARNING_HPP
