/* SPDX-License-Identifier: MIT */

#ifndef EXTERN_VERSION_H
#define EXTERN_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define PACKAGE_VERSION_MAJOR 0
#define PACKAGE_VERSION_MINOR 6
#define PACKAGE_VERSION_PATCH 1

char const *get_package_version_string(void);

#ifdef __cplusplus
}
#endif

#endif // EXTERN_VERSION_H
