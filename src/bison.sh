#!/bin/sh
set -eu

INPUT_CPP="${1:?}"
OUTPUT_Y="${2:?}"

BISONFLAGS=-Wall

BISON_MAJOR=$(bison -V | sed -E 's/^.+ ([0-9]+)\..*$/\1/g;q')
BISON_MINOR=$(bison -V | sed -E 's/^.+ [0-9]+\.([0-9]+)\..*$/\1/g;q')

add_flag () {
	if [ "$BISON_MAJOR" -eq "$1" ] && [ "$BISON_MINOR" -ge "$2" ]; then
		BISONFLAGS="$BISONFLAGS -D$3"
	fi
}

add_flag 3 0 parse.error=verbose
add_flag 3 0 parse.lac=full
add_flag 3 0 lr.type=ielr
add_flag 3 5 api.token.raw=true
add_flag 3 6 parse.error=detailed

echo "BISONFLAGS='$BISONFLAGS'"

# Replace our own arguments ($@) with the ones in $BISONFLAGS
eval "set -- $BISONFLAGS"

exec bison $@ -d -o "$INPUT_CPP" "$OUTPUT_Y"
