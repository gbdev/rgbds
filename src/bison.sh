#!/usr/bin/env bash
set -e

BISONFLAGS=-Wall

readonly BISON_MAJOR=$(bison -V | sed -E 's/^.+ ([0-9]+)\..*$/\1/g;q')
readonly BISON_MINOR=$(bison -V | sed -E 's/^.+ [0-9]+\.([0-9]+)\..*$/\1/g;q')

add_flag () {
	if [[ "$BISON_MAJOR" -eq "$1" && "$BISON_MINOR" -ge "$2" ]]; then
		BISONFLAGS="$BISONFLAGS -D$3"
	fi
}

add_flag 3 0 parse.error=verbose
add_flag 3 0 parse.lac=full
add_flag 3 0 lr.type=ielr
add_flag 3 5 api.token.raw=true
add_flag 3 6 parse.error=detailed

echo "BISONFLAGS=$BISONFLAGS"

exec bison $BISONFLAGS -d -o "$1" "$2"
