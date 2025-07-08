// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_WARNING_HPP
#define RGBDS_LINK_WARNING_HPP

#include <stdint.h>

struct FileStackNode;

[[gnu::format(printf, 3, 4)]]
void warning(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 3, 4)]]
void error(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 3, 4)]]
void errorNoNewline(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 2, 3)]]
void argErr(char flag, char const *fmt, ...);
[[gnu::format(printf, 3, 4), noreturn]]
void fatal(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);

void requireZeroErrors();

#endif // RGBDS_LINK_WARNING_HPP
