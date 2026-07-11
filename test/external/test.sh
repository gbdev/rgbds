#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

cd "$(dirname "$0")"

NAME="$1"
shift # Any remaining arguments get forwarded to `make`.

if [ ! -f "$NAME.cfg" ]; then
	echo >&2 'External test file '"$NAME"'.cfg does not exist'
	exit 1
fi

# Sourcing "$NAME.cfg" defines `EXT_TEST_*` variables used below.
. "$NAME.cfg"

if ! cd "$EXT_TEST_REPO"; then
	echo >&2 'Please fetch test deps before running any external test'
	exit 1
fi

RGBDS_PATH="RGBDS=../../../"
git clean -fdx # Clean any previous build products so `make` rebuilds everything from scratch.
make "$@" "$EXT_TEST_TARGET" $RGBDS_PATH

hash="$(sha1sum -b "$EXT_TEST_FILE" | head -c 40)"
if [ "$hash" != "$EXT_TEST_HASH" ]; then
	cat >&2 <<EOM
error: "$EXT_TEST_FILE" checksum did not match!
    Expected $EXT_TEST_HASH,
         got $hash
EOM
	exit 1
fi
