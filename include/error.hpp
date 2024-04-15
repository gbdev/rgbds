/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ERROR_HPP
#define RGBDS_ERROR_HPP

extern "C" {

[[gnu::format(printf, 1, 2)]] void warn(char const *fmt...);
[[gnu::format(printf, 1, 2)]] void warnx(char const *fmt, ...);

[[gnu::format(printf, 1, 2), noreturn]] void err(char const *fmt, ...);
[[gnu::format(printf, 1, 2), noreturn]] void errx(char const *fmt, ...);
}

#endif // RGBDS_ERROR_HPP
