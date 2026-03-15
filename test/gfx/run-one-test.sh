#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run a single RGBGFX test case in an isolated temp directory.
#
# Usage:
#   run-one-test.sh <test-type> <test-name> [extra-args...]
#
# test-type:
#   png      — a .png file test (runs normal + piped variants)
#   seed     — a seed*.bin test (runs 4 flag variants)
#   reverse  — a .1bpp/.2bpp round-trip test
#   stdout   — the write-stdout test
#
# TEST_SRCDIR must point to the test/gfx source directory.

set -euo pipefail

# shellcheck source=../helpers.sh
source "$(dirname "$0")/../helpers.sh"

TEST_SRCDIR="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
setup_tools
setup_tmpdir

test_type="${1:?missing test-type argument}"
test_name="${2:?missing test-name argument}"
shift 2

rc=0

cd "$TEST_SRCDIR"

RGBGFX="${RGBGFX}"

case "$test_type" in

# =========================================================================
# PNG tests — normal + piped variants
# =========================================================================
png)
	f="${test_name}.png"
	base="${test_name}"

	flags="$([[ -e "${base}.flags" ]] && echo "@${base}.flags" || true)"

	# Build output flags — write result files to tmpdir
	for f_ext in o_1bpp o_2bpp p_pal t_tilemap a_attrmap q_palmap; do
		if [[ -e "${base}.out.${f_ext#*_}" ]]; then
			flags="$flags -${f_ext%_*} $TMPWORKDIR/result.${f_ext#*_}"
		fi
	done

	errtmp="$TMPWORKDIR/stderr"

	# --- normal variant ---
	echo "${bold}${green}Testing: $RGBGFX $flags $f${rescolors}${resbold}"
	if [[ -e "${base}.err" ]]; then
		"$RGBGFX" $flags "$f" 2>"$errtmp" || true
		diff -au --strip-trailing-cr "${base}.err" "$errtmp" || { rc=1; echo "${bold}${red}Test $RGBGFX $flags $f failed!${rescolors}${resbold}" >&2; }
	else
		if ! "$RGBGFX" $flags "$f" 2>"$errtmp"; then
			rc=1
			echo "${bold}${red}Test $RGBGFX $flags $f failed!${rescolors}${resbold}" >&2
			cat "$errtmp" >&2
		else
			# Check outputs
			for ext in 1bpp 2bpp pal tilemap attrmap palmap; do
				if [[ -e "${base}.out.$ext" ]]; then
					tryCmp "${base}.out.$ext" "$TMPWORKDIR/result.$ext" "${base}.out.$ext" || rc=1
				fi
			done
		fi
	fi

	# --- piped variant ---
	# Re-build flags for tmpdir
	flags="$([[ -e "${base}.flags" ]] && echo "@${base}.flags" || true)"
	for f_ext in o_1bpp o_2bpp p_pal t_tilemap a_attrmap q_palmap; do
		if [[ -e "${base}.out.${f_ext#*_}" ]]; then
			flags="$flags -${f_ext%_*} $TMPWORKDIR/result.${f_ext#*_}"
		fi
	done

	echo "${bold}${green}Testing: $RGBGFX $flags - <$f${rescolors}${resbold}"
	if [[ -e "${base}.err" ]]; then
		"$RGBGFX" $flags - <"$f" 2>"$errtmp" || true
		diff -au --strip-trailing-cr <(sed "s/$f/<stdin>/g" "${base}.err") "$errtmp" || { rc=1; echo "${bold}${red}Test $RGBGFX $flags - <$f failed!${rescolors}${resbold}" >&2; }
	else
		if ! "$RGBGFX" $flags - <"$f" 2>"$errtmp"; then
			rc=1
			echo "${bold}${red}Test $RGBGFX $flags - <$f failed!${rescolors}${resbold}" >&2
			cat "$errtmp" >&2
		else
			for ext in 1bpp 2bpp pal tilemap attrmap palmap; do
				if [[ -e "${base}.out.$ext" ]]; then
					tryCmp "${base}.out.$ext" "$TMPWORKDIR/result.$ext" "${base}.out.$ext (piped)" || rc=1
				fi
			done
		fi
	fi
	;;

# =========================================================================
# Seed tests — 4 flag combinations
# Run in an isolated tmpdir because rgbgfx_test writes out0.png, result.*
# to CWD and calls ../../rgbgfx relative to CWD.
# =========================================================================
seed)
	f="${test_name}"

	# Set up tmpdir with directory structure so ../../rgbgfx resolves:
	#   $TMPWORKDIR/rgbgfx          →  symlink to real rgbgfx binary
	#   $TMPWORKDIR/a/b/rgbgfx_test →  symlink to real rgbgfx_test
	#   $TMPWORKDIR/a/b/randtilegen →  symlink to real randtilegen
	#   $TMPWORKDIR/a/b/seed*.bin   →  symlink to test input
	# CWD = $TMPWORKDIR/a/b/ so ../../rgbgfx = $TMPWORKDIR/rgbgfx ✓
	seed_workdir="$TMPWORKDIR/a/b"
	mkdir -p "$seed_workdir"
	ln -sf "$(cd "$TEST_SRCDIR/../.." && pwd)/rgbgfx" "$TMPWORKDIR/rgbgfx"
	ln -sf "$TEST_SRCDIR/rgbgfx_test" "$seed_workdir/rgbgfx_test"
	ln -sf "$TEST_SRCDIR/randtilegen" "$seed_workdir/randtilegen"
	ln -sf "$TEST_SRCDIR/$f" "$seed_workdir/$f"
	cd "$seed_workdir"

	# Draw a random tile offset and VRAM0 size
	ofs=0
	size=0
	while [[ "$ofs" -eq 0 ]]; do (( ofs = RANDOM % 256 )); done
	while [[ "$size" -eq 0 ]]; do (( size = RANDOM % 256 )); done

	for flags in ""{," -b $ofs"}{," -N $size,256"}; do
		echo "${bold}${green}Testing: ./rgbgfx_test $f $flags${rescolors}${resbold}"
		if ! ./rgbgfx_test "$f" $flags; then
			rc=1
			echo "${bold}${red}Test ./rgbgfx_test $f $flags failed!${rescolors}${resbold}" >&2
		fi
	done
	;;

# =========================================================================
# Reverse tests — round-trip
# =========================================================================
reverse)
	f="${test_name}"
	base="${f%.[12]bpp}"

	flags="$([[ -e "${base}.flags" ]] && echo "@${base}.flags" || true) $([[ "${f}" = *.1bpp ]] && echo "-d 1" || true)"

	result_png="$TMPWORKDIR/result.png"
	result_2bpp="$TMPWORKDIR/result.2bpp"

	echo "${bold}${green}Testing: $RGBGFX $flags -o $f -r 1 result.png && $RGBGFX $flags -o result.2bpp result.png${rescolors}${resbold}"
	if ! ($RGBGFX $flags -o "$f" -r 1 "$result_png" && $RGBGFX $flags -o "$result_2bpp" "$result_png"); then
		rc=1
		echo "${bold}${red}Reverse test for $f failed!${rescolors}${resbold}" >&2
	else
		tryCmp "$f" "$result_2bpp" "$f round-trip" || rc=1
	fi
	;;

# =========================================================================
# Write-stdout test
# =========================================================================
stdout)
	result="$TMPWORKDIR/result.2bpp"

	echo "${bold}${green}Testing: $RGBGFX -m -o - write_stdout.bin > result.2bpp${rescolors}${resbold}"
	if ! "$RGBGFX" -m -o - write_stdout.bin > "$result"; then
		rc=1
		echo "${bold}${red}Write-stdout test failed!${rescolors}${resbold}" >&2
	else
		tryCmp write_stdout.out.2bpp "$result" "write_stdout round-trip" || rc=1
	fi
	;;

*)
	echo "Unknown test type: $test_type" >&2
	exit 2
	;;
esac

exit $rc
