#!/bin/bash

export LC_ALL=C
set -o pipefail

otemp="$(mktemp)"
gbtemp="$(mktemp)"
gbtemp2="$(mktemp)"
outtemp="$(mktemp)"
rc=0

trap "rm -f '$otemp' '$gbtemp' '$gbtemp2' '$outtemp'" EXIT

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBASM=../../rgbasm
RGBLINK=../../rgblink

startTest () {
	echo "$bold$green${i%.asm}...$rescolors$resbold"
}

tryDiff () {
	if ! diff -u --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}${i%.asm}.out mismatch!${rescolors}${resbold}"
		false
	fi
}

tryCmp () {
	if ! cmp "$1" "$2"; then
		../../contrib/gbdiff.bash "$1" "$2"
		echo "${bold}${red}${i%.asm}.out.bin mismatch!${rescolors}${resbold}"
		false
	fi
}

rgblink() {
	out="$(env $RGBLINK "$@")" || return $?
	if [[ -n "$out" ]]; then
		echo "$bold${red}Linking shouldn't produce anything on stdout!$rescolors$resbold"
		false
	fi
}

for i in *.asm; do
	startTest
	$RGBASM -o $otemp $i

	# Some tests have variants depending on flags
	ran_flag=
	for flag in '-d' '-t' '-w'; do
		if [ -f ${i%.asm}-no${flag}.out ]; then
			rgblink -o $gbtemp $otemp > $outtemp 2>&1
			tryDiff ${i%.asm}-no${flag}.out $outtemp
			rc=$(($? || $rc))
			ran_flag=1
		fi
		if [ -f ${i%.asm}${flag}.out ]; then
			rgblink ${flag} -o $gbtemp $otemp > $outtemp 2>&1
			tryDiff ${i%.asm}${flag}.out $outtemp
			rc=$(($? || $rc))
			ran_flag=1
		fi
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# Other tests have several linker scripts
	find . -name "${i%.asm}*.link" | while read script; do
		rgblink -l $script -o $gbtemp $otemp > $outtemp 2>&1
		tryDiff ${script%.link}.out $outtemp
		rc=$(($? || $rc))
		ran_flag=1
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# The rest of the tests just links a file, and maybe checks the binary
	rgblink -o $gbtemp $otemp > $outtemp 2>&1
	if [ -f ${i%.asm}.out ]; then
		tryDiff ${i%.asm}.out $outtemp
		rc=$(($? || $rc))
	fi

	bin=${i%.asm}.out.bin
	if [ -f $bin ]; then
		dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < $bin)) > $otemp 2>/dev/null
		tryCmp $bin $otemp
		rc=$(($? || $rc))
	fi
done

for i in smart/*.asm; do
	startTest
	$RGBASM -o $otemp $i
	rm $gbtemp
	$RGBLINK -o $gbtemp $otemp
	tryCmp "${i%.asm}.bin" $gbtemp
	rc=$(($? || $rc))
	rm $gbtemp
	i="${i%.asm}.smart"
	startTest
	$RGBLINK -vs "root" -o $gbtemp $otemp
	tryCmp "$i.bin" $gbtemp
	rc=$(($? || $rc))
done

# These tests do their own thing

i="bank-const.asm"
startTest
$RGBASM -o $otemp bank-const/a.asm
$RGBASM -o $gbtemp2 bank-const/b.asm
rgblink -o $gbtemp $gbtemp2 $otemp > $outtemp 2>&1
tryDiff bank-const/out.err $outtemp
rc=$(($? || $rc))

for i in fragment-align/*; do
	startTest
	$RGBASM -o $otemp $i/a.asm
	$RGBASM -o $gbtemp2 $i/b.asm
	rgblink -o $gbtemp $otemp $gbtemp2 2>$outtemp
	tryDiff $i/out.err $outtemp
	rc=$(($? || $rc))
	if [[ -f $i/out.gb ]]; then
		dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < $i/out.gb)) > $otemp 2>/dev/null
		tryCmp $i/out.gb $otemp
		rc=$(($? || $rc))
	fi
done

i="high-low.asm"
startTest
$RGBASM -o $otemp high-low/a.asm
rgblink -o $gbtemp $otemp
$RGBASM -o $otemp high-low/b.asm
rgblink -o $gbtemp2 $otemp
tryCmp $gbtemp $gbtemp2
rc=$(($? || $rc))

i="overlay.asm"
startTest
$RGBASM -o $otemp overlay/a.asm
rgblink -o $gbtemp -t -O overlay/overlay.gb $otemp > $outtemp 2>&1
# This test does not trim its output with 'dd' because it needs to verify the correct output size
tryDiff overlay/out.err $outtemp
rc=$(($? || $rc))
tryCmp overlay/out.gb $gbtemp
rc=$(($? || $rc))

i="section-union/good.asm"
startTest
$RGBASM -o $otemp section-union/good/a.asm
$RGBASM -o $gbtemp2 section-union/good/b.asm
rgblink -o $gbtemp -l section-union/good/script.link $otemp $gbtemp2
dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < section-union/good/ref.out.bin)) > $otemp 2>/dev/null
tryCmp section-union/good/ref.out.bin $otemp
rc=$(($? || $rc))

i="section-union/fragments.asm"
startTest
$RGBASM -o $otemp section-union/fragments/a.asm
$RGBASM -o $gbtemp2 section-union/fragments/b.asm
rgblink -o $gbtemp $otemp $gbtemp2
dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < section-union/fragments/ref.out.bin)) > $otemp 2>/dev/null
tryCmp section-union/fragments/ref.out.bin $otemp
rc=$(($? || $rc))

for i in section-union/*.asm; do
	startTest
	$RGBASM -o $otemp $i
	$RGBASM -o $gbtemp2 $i -DSECOND
	if rgblink $otemp $gbtemp2 2>$outtemp; then
		echo -e "${bold}${red}$i didn't fail to link!${rescolors}${resbold}"
		rc=1
	fi
	echo --- >> $outtemp
	# Ensure RGBASM also errors out
	cat $i - $i <<<'SECOND equs "1"' | $RGBASM - 2>> $outtemp
	tryDiff ${i%.asm}.out $outtemp
	rc=$(($? || $rc))
done

exit $rc
