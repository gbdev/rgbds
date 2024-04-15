/* SPDX-License-Identifier: MIT */

/* This implementation was taken from musl and modified for RGBDS */

#ifndef RGBDS_EXTERN_GETOPT_HPP
#define RGBDS_EXTERN_GETOPT_HPP

extern "C" {

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

#define no_argument       0
#define required_argument 1
#define optional_argument 2

} // extern "C"

#endif // RGBDS_EXTERN_GETOPT_HPP
