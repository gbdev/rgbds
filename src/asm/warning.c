/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/fstack.h"
#include "asm/main.h"
#include "asm/warning.h"

#include "extern/err.h"

unsigned int nbErrors = 0;

enum WarningState {
	WARNING_DEFAULT,
	WARNING_DISABLED,
	WARNING_ENABLED,
	WARNING_ERROR
};

static enum WarningState const defaultWarnings[NB_WARNINGS] = {
	WARNING_DISABLED, /* Invalid args to builtins */
	WARNING_DISABLED, /* Division undefined behavior */
	WARNING_DISABLED, /* Empty entry in `db`, `dw` or `dl` */
	WARNING_DISABLED, /* Constants too large */
	WARNING_DISABLED, /* String too long for internal buffers */
	WARNING_DISABLED, /* Obsolete things */
	WARNING_DISABLED, /* Shifting undefined behavior */
	WARNING_ENABLED,  /* User warnings */
	WARNING_ENABLED,  /* Assertions */
	WARNING_DISABLED, /* Strange shift amount */
	WARNING_ENABLED,  /* Implicit truncation loses some bits */
};

static enum WarningState warningStates[NB_WARNINGS];

static bool warningsAreErrors; /* Set if `-Werror` was specified */

static enum WarningState warningState(enum WarningID id)
{
	/* Check if warnings are globally disabled */
	if (!CurrentOptions.warnings)
		return WARNING_DISABLED;

	/* Get the actual state */
	enum WarningState state = warningStates[id];

	if (state == WARNING_DEFAULT)
		/* The state isn't set, grab its default state */
		state = defaultWarnings[id];

	if (warningsAreErrors && state == WARNING_ENABLED)
		state = WARNING_ERROR;

	return state;
}

static char const *warningFlags[NB_WARNINGS_ALL] = {
	"builtin-args",
	"div",
	"empty-entry",
	"large-constant",
	"long-string",
	"obsolete",
	"shift",
	"user",
	"assert",
	"shift-amount",
	"truncation",

	/* Meta warnings */
	"all",
	"extra",
	"everything" /* Especially useful for testing */
};

enum MetaWarningCommand {
	META_WARNING_DONE = NB_WARNINGS
};

/* Warnings that probably indicate an error */
static uint8_t const _wallCommands[] = {
	WARNING_BUILTIN_ARG,
	WARNING_LARGE_CONSTANT,
	WARNING_LONG_STR,
	META_WARNING_DONE
};

/* Warnings that are less likely to indicate an error */
static uint8_t const _wextraCommands[] = {
	WARNING_EMPTY_ENTRY,
	WARNING_OBSOLETE,
	META_WARNING_DONE
};

/* Literally everything. Notably useful for testing */
static uint8_t const _weverythingCommands[] = {
	WARNING_BUILTIN_ARG,
	WARNING_DIV,
	WARNING_EMPTY_ENTRY,
	WARNING_LARGE_CONSTANT,
	WARNING_LONG_STR,
	WARNING_OBSOLETE,
	WARNING_SHIFT,
	WARNING_USER,
	WARNING_SHIFT_AMOUNT,
	META_WARNING_DONE
};

static uint8_t const *metaWarningCommands[NB_META_WARNINGS] = {
	_wallCommands,
	_wextraCommands,
	_weverythingCommands
};

void processWarningFlag(char const *flag)
{
	static bool setError = false;

	/* First, try to match against a "meta" warning */
	for (enum WarningID id = NB_WARNINGS; id < NB_WARNINGS_ALL; id++) {
		/* TODO: improve the matching performance? */
		if (!strcmp(flag, warningFlags[id])) {
			/* We got a match! */
			if (setError)
				errx(1, "Cannot make meta warning \"%s\" into an error",
				     flag);

			uint8_t const *ptr =
					metaWarningCommands[id - NB_WARNINGS];

			for (;;) {
				if (*ptr == META_WARNING_DONE)
					return;

				/* Warning flag, set without override */
				if (warningStates[*ptr] == WARNING_DEFAULT)
					warningStates[*ptr] = WARNING_ENABLED;
				ptr++;
			}
		}
	}

	/* If it's not a meta warning, specially check against `-Werror` */
	if (!strncmp(flag, "error", strlen("error"))) {
		char const *errorFlag = flag + strlen("error");

		switch (*errorFlag) {
		case '\0':
			/* `-Werror` */
			warningsAreErrors = true;
			return;

		case '=':
			/* `-Werror=XXX */
			setError = true;
			processWarningFlag(errorFlag + 1); /* Skip the `=` */
			setError = false;
			return;

		/* Otherwise, allow parsing as another flag */
		}
	}

	/* Well, it's either a normal warning or a mistake */

	/* Check if this is a negation */
	bool isNegation = !strncmp(flag, "no-", strlen("no-")) && !setError;
	char const *rootFlag = isNegation ? flag + strlen("no-") : flag;
	enum WarningState state = setError ? WARNING_ERROR :
			isNegation ? WARNING_DISABLED : WARNING_ENABLED;

	/* Try to match the flag against a "normal" flag */
	for (enum WarningID id = 0; id < NB_WARNINGS; id++) {
		if (!strcmp(rootFlag, warningFlags[id])) {
			/* We got a match! */
			warningStates[id] = state;
			return;
		}
	}

	warnx("Unknown warning `%s`", flag);
}

void verror(const char *fmt, va_list args, char const *flag)
{
	fputs("ERROR: ", stderr);
	fstk_Dump();
	fprintf(stderr, flag ? ": [-Werror=%s]\n    " : ":\n    ", flag);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	fstk_DumpStringExpansions();
	nbErrors++;
}

void yyerror(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	verror(fmt, args, NULL);
	va_end(args);
}

noreturn_ void fatalerror(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	verror(fmt, args, NULL);
	va_end(args);

	exit(1);
}

void warning(enum WarningID id, char const *fmt, ...)
{
	char const *flag = warningFlags[id];
	va_list args;

	va_start(args, fmt);

	switch (warningState(id)) {
	case WARNING_DISABLED:
		return;

	case WARNING_ERROR:
		verror(fmt, args, flag);
		va_end(args);
		return;

	case WARNING_DEFAULT:
		trap_;
		/* Not reached */

	case WARNING_ENABLED:
		break;
	}

	fputs("warning: ", stderr);
	fstk_Dump();
	fprintf(stderr, ": [-W%s]\n    ", flag);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	fstk_DumpStringExpansions();

	va_end(args);
}
