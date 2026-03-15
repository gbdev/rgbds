#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run a single RGBLINK test case in an isolated temp directory.
#
# Usage:
#   run-one-test.sh <test-type> <test-name>
#
# test-type:
#   simple    — a top-level .asm file (generic loop: flag/script/simple variants)
#   special   — a hardcoded special sub-directory test
#   error     — section-union/*.asm / section-fragment/*.asm error tests
#
# TEST_SRCDIR must point to the test/link source directory.

set -euo pipefail
set -o pipefail

# shellcheck source=../helpers.sh
source "$(dirname "$0")/../helpers.sh"

TEST_SRCDIR="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
setup_tools
setup_tmpdir

test_type="${1:?missing test-type argument}"
test_name="${2:?missing test-name argument}"

rc=0

cd "$TEST_SRCDIR"

# Temp file aliases inside the isolated tmpdir
otemp="$TMPWORKDIR/a.o"
gbtemp="$TMPWORKDIR/out.gb"
gbtemp2="$TMPWORKDIR/b.o"
outtemp="$TMPWORKDIR/out1"
outtemp2="$TMPWORKDIR/out2"
outtemp3="$TMPWORKDIR/out3"

case "$test_type" in

# =========================================================================
# Simple .asm tests — the generic loop from test.sh
# =========================================================================
simple)
	test="$test_name"

	echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
	"$RGBASM" -o "$otemp" "${test}.asm"

	our_rc=0

	# ---------- variant flag tests ----------
	ran_flag=false
	for flag in '-d' '-t' '-w'; do
		if [ -f "${test}-no${flag}.out" ]; then
			echo "${bold}${green}${test}-no${flag}...${rescolors}${resbold}"
			rgblinkQuiet -o "$gbtemp" "$otemp" 2>"$outtemp" || true
			tryDiff "${test}-no${flag}.out" "$outtemp" "${test}-no${flag}.out" || our_rc=1
			ran_flag=true
		fi
		if [ -f "${test}${flag}.out" ]; then
			echo "${bold}${green}${test}${flag}...${rescolors}${resbold}"
			rgblinkQuiet ${flag} -o "$gbtemp" "$otemp" 2>"$outtemp" || true
			tryDiff "${test}${flag}.out" "$outtemp" "${test}${flag}.out" || our_rc=1
			ran_flag=true
		fi
	done
	if "$ran_flag"; then
		rc=$our_rc
		exit $rc
	fi

	# ---------- linker script tests ----------
	ran_flag=false
	for script in "$test"*.link; do
		[[ -e "$script" ]] || break
		echo "${bold}${green}${test} ${script#${test}}...${rescolors}${resbold}"
		rgblinkQuiet -l "$script" -o "$gbtemp" "$otemp" 2>"$outtemp" || true
		tryDiff "${script%.link}.out" "$outtemp" "${script%.link}.out" || our_rc=1
		ran_flag=true
	done
	if "$ran_flag"; then
		rc=$our_rc
		exit $rc
	fi

	# ---------- simple link + compare ----------
	echo "${bold}${green}${test}...${rescolors}${resbold}"
	rgblinkQuiet -o "$gbtemp" "$otemp" 2>"$outtemp" || true
	tryDiff "${test}.out" "$outtemp" "${test}.out" || our_rc=1
	bin="${test}.out.bin"
	if [ -f "$bin" ]; then
		tryCmpRom "$bin" "$gbtemp" "${test}.out.bin" || our_rc=1
	fi
	rc=$our_rc
	;;

# =========================================================================
# Special sub-directory tests
# =========================================================================
special)
	test="$test_name"
	our_rc=0

	case "$test" in

	bank-const)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	constant-parent)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -n "$outtemp2" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp2" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	export-all)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -E -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -n "$outtemp2" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp2" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	fragment-align/*)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		if [[ -f "$test"/out.gb ]]; then
			tryCmpRom "$test"/out.gb "$gbtemp" || our_rc=1
		fi
		;;

	fragment-literals)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -m "$outtemp" -n "$outtemp2" "$otemp" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		tryDiff "$test"/ref.out.map "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp2" || our_rc=1
		;;

	high-low)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$outtemp" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" || true
		rgblinkQuiet -o "$gbtemp2" "$outtemp" || true
		tryCmp "$gbtemp" "$gbtemp2" || our_rc=1
		;;

	load-fragment/base)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -n "$outtemp" "$otemp" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp" || our_rc=1
		;;

	load-fragment/multiple-objects)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	load-fragment/section-fragment)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$outtemp" "$test"/b.asm
		"$RGBASM" -o "$outtemp2" "$test"/c.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -m "$outtemp3" -n "$gbtemp2" "$otemp" "$outtemp" "$outtemp2" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		tryDiff "$test"/ref.out.map "$outtemp3" || our_rc=1
		tryDiff "$test"/ref.out.sym "$gbtemp2" || our_rc=1
		;;

	map-file)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -m "$outtemp2" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.map "$outtemp2" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	overlay/smaller)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -p 0x42 -O "$test"/overlay.gb "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryCmp "$test"/out.gb "$gbtemp" || our_rc=1
		;;

	overlay/unfixed)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -O "$test"/overlay.gb "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	overlay/tiny)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -t -O "$test"/overlay.gb "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryCmp "$test"/out.gb "$gbtemp" || our_rc=1
		;;

	pipeline)
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		("$RGBASM" -o - - | "$RGBLINK" -o - - | "$RGBFIX" -v -p 0xff -) < "$test"/a.asm > "$gbtemp"
		tryCmp "$test"/out.gb "$gbtemp" || our_rc=1
		;;

	rept-trace)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -Bno-collapse -o "$gbtemp" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	same-consts)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	scramble-invalid)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -S "romx := 4" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	scramble-romx)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -S "romx=3,wramx=4,sram=4" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryCmpRomSize "$gbtemp" 65536 || our_rc=1
		;;

	script-include)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -l "$test"/script.link "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	sdcc/good)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -n "$outtemp2" -l "$test"/script.link "$otemp" "$test"/b.rel "$test"/c.rel 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp2" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	sdcc/no-script)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$test"/b.rel 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	section-conflict/different-mod)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$gbtemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	section-fragment/good)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	section-fragment/jr-offset|section-fragment/jr-offset-load)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	section-normal/same-name)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$gbtemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	section-union/compat|section-union/good)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -l "$test"/script.link "$otemp" "$gbtemp2" || true
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	section-union/same-export)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	section-union/same-label)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	symbols/conflict)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$gbtemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	symbols/good)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp2" "$test"/b.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -o "$gbtemp" -n "$outtemp2" "$otemp" "$gbtemp2" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		tryDiff "$test"/ref.out.sym "$outtemp2" || our_rc=1
		tryCmpRom "$test"/ref.out.bin "$gbtemp" || our_rc=1
		;;

	symbols/unknown)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		"$RGBASM" -o "$gbtemp" "$test"/b.asm
		"$RGBASM" -o "$gbtemp2" "$test"/c.asm
		"$RGBASM" -o "$outtemp" "$test"/d.asm
		"$RGBASM" -o "$outtemp2" "$test"/e.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet "$otemp" "$gbtemp" "$gbtemp2" "$outtemp" "$outtemp2" 2>"$outtemp3" || true
		tryDiff "$test"/out.err "$outtemp3" || our_rc=1
		;;

	truncation/level1)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -Wtruncation=1 -o "$gbtemp" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	truncation/level2)
		echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
		"$RGBASM" -o "$otemp" "$test"/a.asm
		echo "${bold}${green}${test}...${rescolors}${resbold}"
		rgblinkQuiet -Wtruncation=2 -o "$gbtemp" "$otemp" 2>"$outtemp" || true
		tryDiff "$test"/out.err "$outtemp" || our_rc=1
		;;

	*)
		echo "Unknown special test: $test" >&2
		exit 2
		;;
	esac

	rc=$our_rc
	;;

# =========================================================================
# Error tests — section-union/*.asm and section-fragment/*.asm
# =========================================================================
error)
	test="$test_name"

	echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
	"$RGBASM" -o "$otemp" "${test}.asm"
	"$RGBASM" -o "$gbtemp2" "${test}.asm" -DSECOND

	echo "${bold}${green}${test}...${rescolors}${resbold}"
	our_rc=0
	if rgblinkQuiet "$otemp" "$gbtemp2" 2>"$outtemp"; then
		echo "${bold}${red}${test}.asm didn't fail to link!${rescolors}${resbold}" >&2
		our_rc=1
	fi
	echo --- >>"$outtemp"
	# Ensure RGBASM also errors out
	cat "${test}.asm" - "${test}.asm" <<<'def SECOND equs "1"' | "$RGBASM" - 2>>"$outtemp" || true
	tryDiff "${test}.out" "$outtemp" || our_rc=1

	rc=$our_rc
	;;

*)
	echo "Unknown test type: $test_type" >&2
	exit 2
	;;
esac

exit $rc
