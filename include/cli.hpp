// SPDX-License-Identifier: MIT

#ifndef RGBDS_CLI_HPP
#define RGBDS_CLI_HPP

#include <stdarg.h>
#include <string>

#include "extern/getopt.hpp" // option

void cli_ParseArgs(
    int argc,
    char *argv[],
    char const *shortOpts,
    option const *longOpts,
    void (*parseArg)(int, char *),
    void (*fatal)(char const *, ...)
);

#endif // RGBDS_CLI_HPP
