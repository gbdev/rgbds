#!/bin/sh
set -eu

OUTPUT_CPP="${1:?}"
INPUT_Y="${2:?}"

BISON_MAJOR=$(bison -V | sed -E 's/^.+ ([0-9]+)\..*$/\1/g;q')
BISON_MINOR=$(bison -V | sed -E 's/^.+ [0-9]+\.([0-9]+)\..*$/\1/g;q')

if [ "$BISON_MAJOR" -lt 3 ]; then
	echo "Bison $BISON_MAJOR.$BISON_MINOR is not supported" 1>&2
	exit 1
fi

BISON_FLAGS="-Wall -Dparse.lac=full -Dlr.type=ielr"

# Set some optimization flags on versions that support them
if [ "$BISON_MAJOR" -eq 4 ] || [ "$BISON_MAJOR" -eq 3 ] && [ "$BISON_MINOR" -ge 5 ]; then
	BISON_FLAGS="$BISON_FLAGS -Dapi.token.raw=true"
fi
if [ "$BISON_MAJOR" -eq 4 ] || [ "$BISON_MAJOR" -eq 3 ] && [ "$BISON_MINOR" -ge 6 ]; then
	BISON_FLAGS="$BISON_FLAGS -Dparse.error=detailed"
else
	BISON_FLAGS="$BISON_FLAGS -Dparse.error=verbose"
fi

# Replace the arguments to this script ($@) with the ones in $BISON_FLAGS
eval "set -- $BISON_FLAGS"

exec bison "$@" -d -o "$OUTPUT_CPP" "$INPUT_Y"
