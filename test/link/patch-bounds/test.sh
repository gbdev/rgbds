#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -euo pipefail
export LC_ALL=C

RGBASM=$1
RGBLINK=$2
test_dir=$(dirname "$0")
temp_dir=$(mktemp -d)
trap 'rm -f "$temp_dir"/*; rmdir "$temp_dir"' EXIT

"$RGBASM" -o "$temp_dir/base.o" "$test_dir/base.asm"
"$RGBASM" -o "$temp_dir/prefix.o" "$test_dir/prefix.asm"

# This fixture has one word patch at offset 0, with a five-byte RPN_SYM
# expression, followed by the assertion count (0). Check that layout before
# mutating fields at fixed distances from the end of the object file.
printf '\0\0\0\0\0\0\0\0\0\0\0\0\1\5\0\0\0\201\0\0\0\0\0\0\0\0' >"$temp_dir/tail"
tail -c 26 "$temp_dir/base.o" >"$temp_dir/actual"
cmp "$temp_dir/tail" "$temp_dir/actual"
object_size=$(wc -c <"$temp_dir/base.o")

writeByte() {
	printf '%b' "$(printf '\\%03o' "$1")"
}

mutatePatch() {
	local offset=$1 type=$2
	cp "$temp_dir/base.o" "$temp_dir/patch.o"
	for shift in 0 8 16 24; do
		writeByte "$(( (offset >> shift) & 255 ))"
	done | dd of="$temp_dir/patch.o" bs=1 seek="$((object_size - 26))" conv=notrunc 2>/dev/null
	writeByte "$type" | dd of="$temp_dir/patch.o" bs=1 seek="$((object_size - 14))" conv=notrunc 2>/dev/null
}

expectFailure() {
	local message=$1
	shift
	# A rejected object must not create or alter the output ROM.
	printf 'unchanged' >"$temp_dir/output.gb"
	local status=0
	"$RGBLINK" -x -o "$temp_dir/output.gb" "$@" >"$temp_dir/stdout" 2>"$temp_dir/stderr" || status=$?
	if [[ $status != 1 ]] || ! grep -Fq "$message" "$temp_dir/stderr"; then
		cat "$temp_dir/stderr"
		echo "Expected rejection (exit 1): $message; got exit $status"
		exit 1
	fi
	test ! -s "$temp_dir/stdout"
	printf 'unchanged' >"$temp_dir/expected"
	cmp "$temp_dir/expected" "$temp_dir/output.gb"
}

# Exercise every patch width, both alone and as the first or a later fragment.
for type in 0 1 2 3; do
	case $type in
		0 | 3) width=1 ;;
		1) width=2 ;;
		2) width=4 ;;
	esac
	for offset in 0xffffffff 0xfffffffe 5 4 "$((5 - width))"; do
		mutatePatch "$offset" "$type"
		message="Patch of size $width at offset \$$(printf '%x' "$offset") writes past the end of section \"test\" (4 bytes long)"
		expectFailure "$message" "$temp_dir/patch.o"
		expectFailure "$message" "$temp_dir/prefix.o" "$temp_dir/patch.o"
		expectFailure "$message" "$temp_dir/patch.o" "$temp_dir/prefix.o"
	done

	# Ending exactly at the fragment boundary is valid, including for JR.
	mutatePatch "$((4 - width))" "$type"
	"$RGBLINK" -x -o "$temp_dir/output.gb" "$temp_dir/prefix.o" "$temp_dir/patch.o" 2>"$temp_dir/stderr"
	test ! -s "$temp_dir/stderr"
	case $type in
		0) printf '\252\0\0\273\3' ;;
		1) printf '\252\0\0\3\0' ;;
		2) printf '\252\3\0\0\0' ;;
		3) printf '\252\0\0\273\0' ;;
	esac >"$temp_dir/expected"
	cmp "$temp_dir/expected" "$temp_dir/output.gb"
done

# Type validation must precede width lookup.
mutatePatch 0xffffffff 255
expectFailure 'has unknown type 0xff' "$temp_dir/patch.o"

# The unmodified word patch still resolves the target in the second fragment.
"$RGBLINK" -x -o "$temp_dir/output.gb" "$temp_dir/prefix.o" "$temp_dir/base.o" 2>"$temp_dir/stderr"
test ! -s "$temp_dir/stderr"
printf '\252\3\0\273\273' >"$temp_dir/expected"
cmp "$temp_dir/expected" "$temp_dir/output.gb"
