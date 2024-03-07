#!/usr/bin/env bash

export LC_ALL=C
set -o pipefail

otemp="$(mktemp)"
gbtemp="$(mktemp)"
gbtemp2="$(mktemp)"
outtemp="$(mktemp)"
tests=0
failed=0
rc=0

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${otemp@Q} ${gbtemp@Q} ${gbtemp2@Q} ${outtemp@Q}" EXIT

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBASM=../../rgbasm
RGBLINK=../../rgblink

startTest () {
	echo "${bold}${green}${i%.asm} assembling...${rescolors}${resbold}"
}

continueTest () {
	(( tests++ ))
	our_rc=0
	echo "${bold}${green}${i%.asm}$1...${rescolors}${resbold}"
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
		echo "${bold}${red}${i%.asm}.out.bin mismatch!${rescolors}${resbold}"
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
		echo "$bold${red}${i%.asm} binary size mismatch! ${rescolors}${resbold}"
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
	startTest
	"$RGBASM" -o "$otemp" "$i"

	# Some tests have variants depending on flags
	ran_flag=false
	for flag in '-d' '-t' '-w'; do
		if [ -f "${i%.asm}-no${flag}.out" ]; then
			continueTest "-no${flag}"
			rgblinkQuiet -o "$gbtemp" "$otemp" 2>"$outtemp"
			tryDiff "${i%.asm}-no${flag}.out" "$outtemp"
			evaluateTest
			ran_flag=true
		fi
		if [ -f "${i%.asm}${flag}.out" ]; then
			continueTest "$flag"
			rgblinkQuiet ${flag} -o "$gbtemp" "$otemp" 2>"$outtemp"
			tryDiff "${i%.asm}${flag}.out" "$outtemp"
			evaluateTest
			ran_flag=true
		fi
	done
	if "$ran_flag"; then
		continue
	fi

	# Other tests have several linker scripts
	for script in "${i%.asm}"*.link; do
		[[ -e "$script" ]] || break # If the glob doesn't match, it just... doesn't expand!

		continueTest "${script#${i%.asm}}"
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
	tryDiff "${i%.asm}.out" "$outtemp"
	bin=${i%.asm}.out.bin
	if [ -f "$bin" ]; then
		tryCmpRom "$bin"
	fi
	evaluateTest
done

# These tests do their own thing

i="bank-const.asm"
startTest
"$RGBASM" -o "$otemp" bank-const/a.asm
"$RGBASM" -o "$gbtemp2" bank-const/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$gbtemp2" "$otemp" 2>"$outtemp"
tryDiff bank-const/out.err "$outtemp"
evaluateTest

for i in fragment-align/*; do
	startTest
	"$RGBASM" -o "$otemp" "$i"/a.asm
	"$RGBASM" -o "$gbtemp2" "$i"/b.asm
	continueTest
	rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp"
	tryDiff "$i"/out.err "$outtemp"
	if [[ -f "$i"/out.gb ]]; then
		tryCmpRom "$i"/out.gb
	fi
	evaluateTest
done

i="high-low.asm"
startTest
"$RGBASM" -o "$otemp" high-low/a.asm
"$RGBASM" -o "$outtemp" high-low/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp"
rgblinkQuiet -o "$gbtemp2" "$outtemp"
tryCmp "$gbtemp" "$gbtemp2"
evaluateTest

i="overlay.asm"
startTest
"$RGBASM" -o "$otemp" overlay/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -t -O overlay/overlay.gb "$otemp" 2>"$outtemp"
tryDiff overlay/out.err "$outtemp"
# This test does not trim its output with 'dd' because it needs to verify the correct output size
tryCmp overlay/out.gb "$gbtemp"
evaluateTest

i="scramble-romx.asm"
startTest
"$RGBASM" -o "$otemp" scramble-romx/a.asm
continueTest
rgblinkQuiet -o "$gbtemp" -S romx=3 "$otemp" 2>"$outtemp"
tryDiff scramble-romx/out.err "$outtemp"
# This test does not compare its exact output with 'tryCmpRom' because no scrambling order is guaranteed
tryCmpRomSize "$gbtemp" 65536
evaluateTest

i="section-fragment/jr-offset.asm"
startTest
"$RGBASM" -o "$otemp" section-fragment/jr-offset/a.asm
"$RGBASM" -o "$gbtemp2" section-fragment/jr-offset/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom section-fragment/jr-offset/ref.out.bin
evaluateTest

i="section-union/good.asm"
startTest
"$RGBASM" -o "$otemp" section-union/good/a.asm
"$RGBASM" -o "$gbtemp2" section-union/good/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" -l section-union/good/script.link "$otemp" "$gbtemp2"
tryCmpRom section-union/good/ref.out.bin
evaluateTest

i="section-union/fragments.asm"
startTest
"$RGBASM" -o "$otemp" section-union/fragments/a.asm
"$RGBASM" -o "$gbtemp2" section-union/fragments/b.asm
continueTest
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom section-union/fragments/ref.out.bin
evaluateTest

for i in section-union/*.asm; do
	startTest
	"$RGBASM" -o "$otemp" "$i"
	"$RGBASM" -o "$gbtemp2" "$i" -DSECOND
	continueTest
	if rgblinkQuiet "$otemp" "$gbtemp2" 2>"$outtemp"; then
		echo -e "${bold}${red}$i didn't fail to link!${rescolors}${resbold}"
		our_rc=1
	fi
	echo --- >>"$outtemp"
	# Ensure RGBASM also errors out
	cat "$i" - "$i" <<<'def SECOND equs "1"' | "$RGBASM" - 2>>"$outtemp"
	tryDiff "${i%.asm}.out" "$outtemp"
	evaluateTest
done

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
