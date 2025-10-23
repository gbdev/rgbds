// SPDX-License-Identifier: MIT

#ifndef RGBDS_CLI_HPP
#define RGBDS_CLI_HPP

#include <stdarg.h>
#include <string>

#include "extern/getopt.hpp" // option
#include "usage.hpp"

void cli_ParseArgs(
    int argc,
    char *argv[],
    char const *shortOpts,
    option const *longOpts,
    void (*parseArg)(int, char *),
    Usage usage
);

#endif // RGBDS_CLI_HPP
