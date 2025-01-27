// SPDX-License-Identifier: MIT

// This implementation was taken from musl and modified for RGBDS

#ifndef RGBDS_EXTERN_GETOPT_HPP
#define RGBDS_EXTERN_GETOPT_HPP

// clang-format off: vertically align values
static constexpr int no_argument       = 0;
static constexpr int required_argument = 1;
static constexpr int optional_argument = 2;
// clang-format on

extern char *musl_optarg;
extern int musl_optind, musl_opterr, musl_optopt, musl_optreset;

struct option {
	char const *name;
	int has_arg;
	int *flag;
	int val;
};

int musl_getopt_long_only(
    int argc, char **argv, char const *optstring, option const *longopts, int *idx
);

#endif // RGBDS_EXTERN_GETOPT_HPP
