#!/bin/bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
	cat <<EOF >&2
Usage: $0 <palettes.pal> <output.png>
EOF
	exit 1
fi

TMP=$(mktemp -d)
readonly TMP
trap 'rm -rf "$TMP"' EXIT

tile() { for i in {0..7}; do printf "$1"; done }
{ tile '\x00\x00' && tile '\xFF\x00' && tile '\x00\xFF' && tile '\xFF\xFF'; } >"$TMP/tmp.2bpp"

NB_BYTES=$(wc -c <"$1")
(( NB_PALS = NB_BYTES / 8 ))
for (( i = 0; i < NB_PALS; i++ )); do
	printf '\0\1\2\3' >>"$TMP/tmp.tilemap"
	printf $(printf '\\x%x' $i{,,,}) >> "$TMP/tmp.palmap"
done

"${RGBGFX:-${RGBDS+$RGBDS/}rgbgfx}" -r 4 "$2" -o "$TMP/tmp.2bpp" -OTQ -p "$1" -n "$NB_PALS"
