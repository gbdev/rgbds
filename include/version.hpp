/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_VERSION_HPP
#define RGBDS_VERSION_HPP

extern "C" {

#define PACKAGE_VERSION_MAJOR 0
#define PACKAGE_VERSION_MINOR 9
#define PACKAGE_VERSION_PATCH 0
#define PACKAGE_VERSION_RC    1

char const *get_package_version_string();
}

#endif // RGBDS_VERSION_H
