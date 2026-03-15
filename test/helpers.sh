#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Common helpers for per-test runner scripts.
# Source this file, don't execute it.

export LC_ALL=C
# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

# ---------------------------------------------------------------------------
# Terminal colours (safe for non-TTY — tput returns empty strings)
# ---------------------------------------------------------------------------
bold="$(tput bold 2>/dev/null || true)"
resbold="$(tput sgr0 2>/dev/null || true)"
red="$(tput setaf 1 2>/dev/null || true)"
green="$(tput setaf 2 2>/dev/null || true)"
orange="$(tput setaf 3 2>/dev/null || true)"
rescolors="$(tput op 2>/dev/null || true)"

# ---------------------------------------------------------------------------
# Resolve tool paths — honour env vars, fall back to build-tree locations
# relative to the *test source directory* (passed via TEST_SRCDIR or $1).
# ---------------------------------------------------------------------------
setup_tools () {
	local srcdir="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
	local root
	root="$(cd "$srcdir/../.." && pwd)"

	RGBASM="${RGBASM:-$root/rgbasm}"
	RGBLINK="${RGBLINK:-$root/rgblink}"
	RGBFIX="${RGBFIX:-$root/rgbfix}"
	RGBGFX="${RGBGFX:-$root/rgbgfx}"
	GBDIFF="${GBDIFF:-$root/contrib/gbdiff.bash}"

	export RGBASM RGBLINK RGBFIX RGBGFX GBDIFF
}

# ---------------------------------------------------------------------------
# Per-test temporary directory — completely isolated working space.
# Cleaned up automatically on EXIT.
# Sets: TMPWORKDIR
# ---------------------------------------------------------------------------
setup_tmpdir () {
	TMPWORKDIR="$(mktemp -d)"
	# Immediate expansion is the desired behavior.
	# shellcheck disable=SC2064
	trap "rm -rf ${TMPWORKDIR@Q}" EXIT
	export TMPWORKDIR
}

# ---------------------------------------------------------------------------
# Comparison helpers
# ---------------------------------------------------------------------------

# Text diff.  Returns 0 on match, 1 on mismatch (with output to stderr).
tryDiff () {
	if ! diff -au --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}${3:-$1} mismatch!${rescolors}${resbold}" >&2
		return 1
	fi
}

# Binary diff.  Falls back to gbdiff.bash for human-readable output.
tryCmp () {
	if ! cmp "$1" "$2"; then
		"$GBDIFF" "$1" "$2" 2>/dev/null || true
		echo "${bold}${red}${3:-$1} mismatch!${rescolors}${resbold}" >&2
		return 1
	fi
}

# Compare a generated ROM (in $1) against an expected binary ($2).
# The ROM is truncated to the expected file's size before comparison.
tryCmpRom () {
	local expected="$1"
	local rom="$2"
	local tmprom="$TMPWORKDIR/_rom_trunc"
	local rom_size
	rom_size=$(printf %s "$(wc -c <"$expected")")
	dd if="$rom" count=1 bs="$rom_size" of="$tmprom" 2>/dev/null
	tryCmp "$expected" "$tmprom" "${3:-ROM binary}"
}

# Check that a file's size matches an expected value.
tryCmpRomSize () {
	local file="$1"
	local expected_size="$2"
	local actual_size
	actual_size=$(printf %s "$(wc -c <"$file")")
	if [ "$actual_size" -ne "$expected_size" ]; then
		echo "${bold}${red}Binary size mismatch! Expected $expected_size, got $actual_size${rescolors}${resbold}" >&2
		return 1
	fi
}

# Run rgblink, fail if it produces anything on stdout.
rgblinkQuiet () {
	local out
	out="$(env "$RGBLINK" -Weverything -Bcollapse "$@")" || return $?
	if [[ -n "$out" ]]; then
		echo "${bold}${red}Linking shouldn't produce anything on stdout!${rescolors}${resbold}" >&2
		return 1
	fi
}
