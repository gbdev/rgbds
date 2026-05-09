// SPDX-License-Identifier: MIT

// This implementation was taken from musl and modified for RGBDS.

#include "extern/getopt.hpp"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "style.hpp"

char *musl_optarg;
int musl_optind = 1, musl_optopt;

static int musl_optpos;

[[gnu::format(printf, 1, 2)]]
static void musl_getopt_error(char const *fmt, ...) {
	style_Set(stderr, STYLE_RED, true);
	fputs("error: ", stderr);
	style_Reset(stderr);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);
}

static int musl_getopt(int argc, char *argv[], char const *optstring) {
	if (!musl_optind) {
		musl_optpos = 0;
		musl_optind = 1;
	}

	if (musl_optind >= argc || !argv[musl_optind]) {
		return -1;
	}

	char *argi = argv[musl_optind];

	if (argi[0] != '-') {
		if (optstring[0] == '-') {
			musl_optarg = argv[musl_optind++];
			return 1;
		}
		return -1;
	}

	if (!argi[1]) {
		return -1;
	}

	if (argi[1] == '-' && !argi[2]) {
		++musl_optind;
		return -1;
	}

	if (!musl_optpos) {
		++musl_optpos;
	}
	wchar_t c;
	int k = mbtowc(&c, argi + musl_optpos, MB_LEN_MAX);
	if (k < 0) {
		k = 1;
		c = 0xFFFD; // replacement char
	}
	char *optchar = argi + musl_optpos;
	musl_optpos += k;

	if (!argi[musl_optpos]) {
		++musl_optind;
		musl_optpos = 0;
	}

	if (optstring[0] == '-' || optstring[0] == '+') {
		++optstring;
	}

	int i = 0;
	wchar_t d = 0;
	int l;
	do {
		l = mbtowc(&d, optstring + i, MB_LEN_MAX);
		if (l > 0) {
			i += l;
		} else {
			++i;
		}
	} while (l && d != c);

	if (d != c || c == ':') {
		musl_optopt = c;
		if (optstring[0] != ':') {
			musl_getopt_error("Unrecognized option '-%s'", optchar);
		}
		return '?';
	}
	if (optstring[i] == ':') {
		musl_optarg = 0;
		if (optstring[i + 1] != ':' || musl_optpos) {
			musl_optarg = argv[musl_optind++] + musl_optpos;
			musl_optpos = 0;
		}
		if (musl_optind > argc) {
			musl_optopt = c;
			if (optstring[0] == ':') {
				return ':';
			}
			musl_getopt_error("Missing argument for option '-%s'", optchar);
			return '?';
		}
	}
	return c;
}

static void permute(char **argv, int dest, int src) {
	char *tmp = argv[src];
	for (int i = src; i > dest; --i) {
		argv[i] = argv[i - 1];
	}
	argv[dest] = tmp;
}

static int
    musl_getopt_long_core(int argc, char **argv, char const *optstring, option const *longopts) {
	musl_optarg = 0;
	if (char *argi = argv[musl_optind];
	    !longopts || argi[0] != '-'
	    || ((!argi[1] || argi[1] == '-') && (argi[1] != '-' || !argi[2]))) {
		return musl_getopt(argc, argv, optstring);
	}

	bool colon = optstring[optstring[0] == '+' || optstring[0] == '-'] == ':';
	int i = 0, cnt = 0, match = 0;
	char *arg = 0, *opt, *start = argv[musl_optind] + 1;

	for (; longopts[i].name; ++i) {
		char const *name = longopts[i].name;
		opt = start;
		if (*opt == '-') {
			++opt;
		}
		while (*opt && *opt != '=' && *opt == *name) {
			++name;
			++opt;
		}
		if (*opt && *opt != '=') {
			continue;
		}
		arg = opt;
		match = i;
		if (!*name) {
			cnt = 1;
			break;
		}
		++cnt;
	}
	if (cnt == 1 && arg - start == mblen(start, MB_LEN_MAX)) {
		int l = arg - start;
		for (i = 0; optstring[i]; ++i) {
			int j = 0;
			while (j < l && start[j] == optstring[i + j]) {
				++j;
			}
			if (j == l) {
				++cnt;
				break;
			}
		}
	}
	if (cnt == 1) {
		i = match;
		opt = arg;
		++musl_optind;
		if (*opt == '=') {
			if (!longopts[i].has_arg) {
				musl_optopt = longopts[i].val;
				if (colon) {
					return '?';
				}
				musl_getopt_error("Option '--%s' does not take an argument", longopts[i].name);
				return '?';
			}
			musl_optarg = opt + 1;
		} else if (longopts[i].has_arg == required_argument) {
			musl_optarg = argv[musl_optind];
			if (!musl_optarg) {
				musl_optopt = longopts[i].val;
				if (colon) {
					return ':';
				}
				musl_getopt_error("Missing argument for option '--%s'", longopts[i].name);
				return '?';
			}
			++musl_optind;
		}
		if (longopts[i].flag) {
			*longopts[i].flag = longopts[i].val;
			return 0;
		}
		return longopts[i].val;
	}
	if (argv[musl_optind][1] == '-') {
		musl_optopt = 0;
		if (!colon) {
			if (cnt) {
				musl_getopt_error("Ambiguous option '--%s'", argv[musl_optind] + 2);
			} else {
				musl_getopt_error("Unrecognized option '--%s'", argv[musl_optind] + 2);
			}
		}
		++musl_optind;
		return '?';
	}
	return musl_getopt(argc, argv, optstring);
}

int musl_getopt_long_only(int argc, char **argv, char const *optstring, option const *longopts) {
	if (!musl_optind) {
		musl_optpos = 0;
		musl_optind = 1;
	}

	if (musl_optind >= argc || !argv[musl_optind]) {
		return -1;
	}

	int skipped = musl_optind;
	if (optstring[0] != '+' && optstring[0] != '-') {
		int i = musl_optind;
		for (;; ++i) {
			if (i >= argc || !argv[i]) {
				return -1;
			}
			if (argv[i][0] == '-' && argv[i][1]) {
				break;
			}
		}
		musl_optind = i;
	}
	int resumed = musl_optind;
	int ret = musl_getopt_long_core(argc, argv, optstring, longopts);
	if (resumed > skipped) {
		int cnt = musl_optind - resumed;
		for (int i = 0; i < cnt; ++i) {
			permute(argv, skipped, musl_optind - 1);
		}
		musl_optind = skipped + cnt;
	}
	return ret;
}
