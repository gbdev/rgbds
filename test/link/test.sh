#!/usr/bin/env bash

export LC_ALL=C
set -o pipefail

otemp="$(mktemp)"
gbtemp="$(mktemp)"
gbtemp2="$(mktemp)"
outtemp="$(mktemp)"
outtemp2="$(mktemp)"
outtemp3="$(mktemp)"

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${otemp@Q} ${gbtemp@Q} ${gbtemp2@Q} ${outtemp@Q} ${outtemp2@Q} ${outtemp3@Q}" EXIT

tests=0
failed=0
rc=0

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBASM=../../rgbasm
RGBLINK=../../rgblink

startTest () {
	echo "${bold}${green}${test} assembling...${rescolors}${resbold}"
}

continueTest () {
	(( tests++ ))
	our_rc=0
	echo "${bold}${green}${test}$1...${rescolors}${resbold}"
}

tryDiff () {
	if ! diff -u --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}$1 mismatch!${rescolors}${resbold}"
		false
	fi
	(( our_rc = our_rc || $? ))
}

tryCmp () {
	if ! cmp "$1" "$2"; then
		../../contrib/gbdiff.bash "$1" "$2"
		echo "${bold}${red}${test}.out.bin mismatch!${rescolors}${resbold}"
		false
	fi
	(( our_rc = our_rc || $? ))
}

tryCmpRom () {
	# `printf` lets us keep only the first returned word from `wc`.
	rom_size=$(printf %s $(wc -c <"$1"))
	dd if="$gbtemp" count=1 bs="$rom_size" >"$otemp" 2>/dev/null
	tryCmp "$1" "$otemp"
}

tryCmpRomSize () {
	rom_size=$(printf %s $(wc -c <"$1"))
	if [ "$rom_size" -ne "$2" ]; then
		echo "$bold${red}${test} binary size mismatch! ${rescolors}${resbold}"
		false
	fi
	(( our_rc = our_rc || $? ))
}

rgblinkQuiet () {
	out="$(env $RGBLINK "$@")" || return $?
	if [[ -n "$out" ]]; then
		echo "$bold${red}Linking shouldn't produce anything on stdout!${rescolors}${resbold}"
		false
	fi
	(( our_rc = our_rc || $? ))
}

evaluateTest () {
	if [[ "$our_rc" -ne 0 ]]; then
		(( failed++ ))
		rc=1
	fi
}

for i in *.asm; do
	test=${i%.asm}
	startTest
	"$RGBASM" -o "$otemp" "${test}.asm"

	# Some tests have variants depending on flags
	ran_flag=false
	for flag in '-d' '-t' '-w'; do
		if [ -f "${test}-no${flag}.out" ]; then
			continueTest "-no${flag}"
			rgblinkQuiet -o "$gbtemp" "$otemp" 2>"$outtemp"
			tryDiff "${test}-no${flag}.out" "$outtemp"
			evaluateTest
			ran_flag=true
		fi
		if [ -f "${test}${flag}.out" ]; then
			continueTest "$flag"
			rgblinkQuiet ${flag} -o "$gbtemp" "$otemp" 2>"$outtemp"
			tryDiff "${test}${flag}.out" "$outtemp"
			evaluateTest
			ran_flag=true
		fi
	done
	if "$ran_flag"; then
		continue
	fi

	# Other tests have several linker scripts
	for script in "$test"*.link; do
		[[ -e "$script" ]] || break # If the glob doesn't match, it just... doesn't expand!

		continueTest "${script#${test}}"
		rgblinkQuiet -l "$script" -o "$gbtemp" "$otemp" 2>"$outtemp"
		tryDiff "${script%.link}.out" "$outtemp"
		evaluateTest
		ran_flag=true
	done
	if "$ran_flag"; then
		continue
	fi

	# The rest of the tests just links a file, and maybe checks the binary
	continueTest
	rgblinkQuiet -o "$gbtemp" "$otemp" 2>"$outtemp"
	tryDiff "${test}.out" "$outtemp"
	bin=${test}.out.bin
	if [ -f "$bin" ]; then
		tryCmpRom "$bin"
	fi
	evaluateTest
done

# These tests do their own thing

test="bank-const"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

for test in fragment-align/*; do
	startTest
	"$RGBASM" -o "$otemp" "$test"/a.asm
	"$RGBASM" -o "$gbtemp2" "$test"/b.asm
	continueTest
	rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp"
	tryDiff "$test"/out.err "$outtemp"
	if [[ -f "$test"/out.gb ]]; then
		tryCmpRom "$test"/out.gb
	fi
	evaluateTest
done

test="high-low"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$outtemp" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp"
rgblinkQuiet -o "$gbtemp2" "$outtemp"
tryCmp "$gbtemp" "$gbtemp2"
evaluateTest

test="map-file"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -m "$outtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
tryDiff "$test"/ref.out.map "$outtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="overlay/smaller"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -p 0x42 -O "$test"/overlay.gb "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
# This test does not trim its output with 'dd' because it needs to verify the correct output size
tryCmp "$test"/out.gb "$gbtemp"
evaluateTest

test="overlay/tiny"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -t -O "$test"/overlay.gb "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
# This test does not trim its output with 'dd' because it needs to verify the correct output size
tryCmp "$test"/out.gb "$gbtemp"
evaluateTest

test="same-consts"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

test="scramble-romx"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -S "romx=3,wramx=4,sram=4" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
# This test does not compare its exact output with 'tryCmpRom' because no scrambling order is guaranteed
tryCmpRomSize "$gbtemp" 65536
evaluateTest

test="sdcc/good"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -n "$outtemp2" -l "$test"/script.link "$otemp" "$test"/b.rel "$test"/c.rel 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
tryDiff "$test"/ref.out.sym "$outtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="sdcc/no-script"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
continueTest
rgblinkQuiet "$otemp" "$test"/b.rel 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

test="section-fragment/good"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="section-fragment/jr-offset"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="section-normal/same-name"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp" "$test"/b.asm
continueTest
rgblinkQuiet "$otemp" "$gbtemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

test="section-union/good"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" -l "$test"/script.link "$otemp" "$gbtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="section-union/same-export"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

test="section-union/same-label"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
tryCmpRom "$test"/ref.out.bin
evaluateTest

for i in section-union/*.asm; do
	test=${i%.asm}
	startTest
	"$RGBASM" -o "$otemp" "${test}.asm"
	"$RGBASM" -o "$gbtemp2" "${test}.asm" -DSECOND
	continueTest
	if rgblinkQuiet "$otemp" "$gbtemp2" 2>"$outtemp"; then
		echo -e "${bold}${red}${test}.asm didn't fail to link!${rescolors}${resbold}"
		our_rc=1
	fi
	echo --- >>"$outtemp"
	# Ensure RGBASM also errors out
	cat "${test}.asm" - "${test}.asm" <<<'def SECOND equs "1"' | "$RGBASM" - 2>>"$outtemp"
	tryDiff "${test}.out" "$outtemp"
	evaluateTest
done

test="symbols/conflict"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp" "$test"/b.asm
continueTest
rgblinkQuiet "$otemp" "$gbtemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
evaluateTest

test="symbols/good"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp2" "$test"/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" -n "$outtemp2" "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff "$test"/out.err "$outtemp"
tryDiff "$test"/ref.out.sym "$outtemp2"
tryCmpRom "$test"/ref.out.bin
evaluateTest

test="symbols/unknown"
startTest
"$RGBASM" -o "$otemp" "$test"/a.asm
"$RGBASM" -o "$gbtemp" "$test"/b.asm
"$RGBASM" -o "$gbtemp2" "$test"/c.asm
"$RGBASM" -o "$outtemp" "$test"/d.asm
"$RGBASM" -o "$outtemp2" "$test"/e.asm
continueTest
rgblinkQuiet "$otemp" "$gbtemp" "$gbtemp2" "$outtemp" "$outtemp2" 2>"$outtemp3"
tryDiff "$test"/out.err "$outtemp3"
evaluateTest

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
