#!/usr/bin/env bash
set -e

BISONFLAGS=-Wall

readonly BISON_VER=$(bison -V)
add_flag () {
	if awk <<<"$BISON_VER" -v major="$1" -v minor="$2" '
	/^bison.*[0-9]+(\.[0-9]+)(\.[0-9]+)?$/ {
		match($0, /[0-9]+(\.[0-9]+)(\.[0-9]+)?$/);
		split(substr($0, RSTART), ver, ".");
		if (ver[1] == major && ver[2] >= minor) { exit 0 } else { exit 1 }
	}'; then
		BISONFLAGS="-D$3 $BISONFLAGS"
	fi
}

add_flag 3 5 api.token.raw=true
add_flag 3 6 parse.error=detailed
add_flag 3 0 parse.error=verbose
add_flag 3 0 parse.lac=full
add_flag 3 0 lr.type=ielr

echo "BISONFLAGS=$BISONFLAGS"

exec bison $BISONFLAGS -d -o "$1" "$2"
