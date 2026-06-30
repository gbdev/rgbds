#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

cd "$(dirname "$0")/.."

RGBDS_PATH="RGBDS=../../"

test_downstream() {
	if ! pushd "$EXTERNAL_TEST_REPO"; then
		echo >&2 'Please fetch test deps before running any external test'
		return 1
	fi
	make clean $RGBDS_PATH
	make -j4 "$EXTERNAL_TEST_TARGET" $RGBDS_PATH
	hash="$(sha1sum -b "$EXTERNAL_TEST_FILE" | head -c 40)"
	if [ "$hash" != "$EXTERNAL_TEST_HASH" ]; then
		echo >&2 'SHA-1 hash of '"$EXTERNAL_TEST_FILE"' did not match: '"$hash"
		return 1
	fi
	popd
}

if [ ! -f "external/$1.cfg" ]; then
	echo >&2 'External test file '"$1"'.cfg does not exist'
	exit 1
fi

# Sourcing "external/$1.cfg" defines `EXTERNAL_TEST_*` values used by `test_downstream`.
. "external/$1.cfg" && test_downstream
