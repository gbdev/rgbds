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
#include <stdlib.h>
#include <string.h>

#include "asm/fstack.h"
#include "asm/main.h"
#include "asm/warning.h"

#include "extern/err.h"

unsigned int nbErrors = 0;

static enum WarningState const defaultWarnings[NB_WARNINGS] = {
	[WARNING_ASSERT]		= WARNING_ENABLED,
	[WARNING_BACKWARDS_FOR]		= WARNING_DISABLED,
	[WARNING_BUILTIN_ARG]		= WARNING_DISABLED,
	[WARNING_CHARMAP_REDEF]		= WARNING_DISABLED,
	[WARNING_DIV]			= WARNING_DISABLED,
	[WARNING_EMPTY_DATA_DIRECTIVE]	= WARNING_DISABLED,
	[WARNING_EMPTY_MACRO_ARG]	= WARNING_DISABLED,
	[WARNING_EMPTY_STRRPL]		= WARNING_DISABLED,
	[WARNING_LARGE_CONSTANT]	= WARNING_DISABLED,
	[WARNING_MACRO_SHIFT]		= WARNING_DISABLED,
	[WARNING_NESTED_COMMENT]	= WARNING_ENABLED,
	[WARNING_OBSOLETE]		= WARNING_ENABLED,
	[WARNING_SHIFT]			= WARNING_DISABLED,
	[WARNING_SHIFT_AMOUNT]		= WARNING_DISABLED,
	[WARNING_TRUNCATION]		= WARNING_ENABLED,
	[WARNING_USER]			= WARNING_ENABLED,
};

enum WarningState warningStates[NB_WARNINGS];

bool warningsAreErrors; /* Set if `-Werror` was specified */

static enum WarningState warningState(enum WarningID id)
{
	/* Check if warnings are globally disabled */
	if (!warnings)
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
	"assert",
	"backwards-for",
	"builtin-args",
	"charmap-redef",
	"div",
	"empty-data-directive",
	"empty-macro-arg",
	"empty-strrpl",
	"large-constant",
	"macro-shift",
	"nested-comment",
	"obsolete",
	"shift",
	"shift-amount",
	"truncation",
	"user",

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
	WARNING_BACKWARDS_FOR,
	WARNING_BUILTIN_ARG,
	WARNING_CHARMAP_REDEF,
	WARNING_EMPTY_DATA_DIRECTIVE,
	WARNING_EMPTY_STRRPL,
	WARNING_LARGE_CONSTANT,
	WARNING_LONG_STR,
	META_WARNING_DONE
};

/* Warnings that are less likely to indicate an error */
static uint8_t const _wextraCommands[] = {
	WARNING_EMPTY_MACRO_ARG,
	WARNING_MACRO_SHIFT,
	WARNING_NESTED_COMMENT,
	META_WARNING_DONE
};

/* Literally everything. Notably useful for testing */
static uint8_t const _weverythingCommands[] = {
	WARNING_BACKWARDS_FOR,
	WARNING_BUILTIN_ARG,
	WARNING_DIV,
	WARNING_EMPTY_DATA_DIRECTIVE,
	WARNING_EMPTY_MACRO_ARG,
	WARNING_EMPTY_STRRPL,
	WARNING_LARGE_CONSTANT,
	WARNING_LONG_STR,
	WARNING_MACRO_SHIFT,
	WARNING_NESTED_COMMENT,
	WARNING_OBSOLETE,
	WARNING_SHIFT,
	WARNING_SHIFT_AMOUNT,
	/* WARNING_TRUNCATION, */
	/* WARNING_USER, */
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

			uint8_t const *ptr = metaWarningCommands[id - NB_WARNINGS];

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

void printDiag(const char *fmt, va_list args, char const *type,
	       char const *flagfmt, char const *flag)
{
	fputs(type, stderr);
	fstk_DumpCurrent();
	fprintf(stderr, flagfmt, flag);
	vfprintf(stderr, fmt, args);
	lexer_DumpStringExpansions();
}

void error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "ERROR: ", ":\n    ", NULL);
	va_end(args);
	nbErrors++;
}

_Noreturn void fatalerror(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "FATAL: ", ":\n    ", NULL);
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
		printDiag(fmt, args, "ERROR: ", ": [-Werror=%s]\n    ", flag);
		va_end(args);
		return;

	case WARNING_DEFAULT:
		unreachable_();
		/* Not reached */

	case WARNING_ENABLED:
		break;
	}

	printDiag(fmt, args, "warning: ", ": [-W%s]\n    ", flag);

	va_end(args);
}
