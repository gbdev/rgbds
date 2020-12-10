#!/bin/sh
bison -V | awk '
/^bison.*[0-9]+(\.[0-9]+){1,2}$/ {
	match($0, /[0-9]+(\.[0-9]+){1,2}$/);
	split(substr($0, RSTART), ver, ".");
	if (ver[1] == major && ver[2] >= minor) { exit 0 } else { exit 1 }
}' major="$1" minor="$2"
