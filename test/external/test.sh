#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

cd "$(dirname "$0")/.."

RGBDS_PATH="RGBDS=../../"

action() { # owner repo make-target build-file build-hash
	if ! pushd "$2"; then
		echo >&2 'Please fetch test deps before running any external test'
		return 1
	fi
	make clean $RGBDS_PATH
	make -j4 "$3" $RGBDS_PATH
	hash="$(sha1sum -b "$4" | head -c 40)"
	if [ "$hash" != "$5" ]; then
		echo >&2 'SHA-1 hash of '"$4"' did not match: '"$hash"
		return 1
	fi
	popd
}

if [ ! -f "external/$1.sh" ]; then
	echo >&2 'External test file '"$1"'.sh does not exist'
	exit 1
fi

# Sourcing "external/$1.sh" defines a `test_action` function, which calls the above
# `action` function with the appropriate arguments for its external repository.
. "external/$1.sh" && test_action
