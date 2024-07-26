/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_UTIL_HPP
#define RGBDS_UTIL_HPP

#include <stddef.h>
#include <stdint.h>
#include <vector>

char const *printChar(int c);

/*
 * @return The number of bytes read, or 0 if invalid data was found
 */
size_t readUTF8Char(std::vector<int32_t> *dest, char const *src);

#endif // RGBDS_UTIL_HPP
