#!/bin/sh
export LC_ALL=C

otemp=$(mktemp)
gbtemp=$(mktemp)
gbtemp2=$(mktemp)
outtemp=$(mktemp)
rc=0

bold=$(tput bold)
resbold=$(tput sgr0)
red=$(tput setaf 1)
rescolors=$(tput op)
tryDiff () {
	diff -u --strip-trailing-cr $1 $2 || (echo -e "${bold}${red}${i%.asm}.out mismatch!${rescolors}${resbold}"; false)
}

tryCmp () {
	cmp $1 $2 || (../../contrib/gbdiff.bash $1 $2; echo -e "${bold}${red}${i%.asm}${variant}.out.bin mismatch!${rescolors}${resbold}"; false)
}

RGBASM=../../rgbasm
RGBLINK=../../rgblink

for i in *.asm; do
	$RGBASM -o $otemp $i

	# Some tests have variants depending on flags
	ran_flag=
	for flag in '-d' '-t' '-w'; do
		if [ -f ${i%.asm}-no${flag}.out ]; then
			$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
			tryDiff ${i%.asm}-no${flag}.out $outtemp
			rc=$(($? || $rc))
			ran_flag=1
		fi
		if [ -f ${i%.asm}${flag}.out ]; then
			$RGBLINK ${flag} -o $gbtemp $otemp > $outtemp 2>&1
			tryDiff ${i%.asm}${flag}.out $outtemp
			rc=$(($? || $rc))
			ran_flag=1
		fi
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# Other tests have several linker scripts
	for script in `find . -name "${i%.asm}*.link"`; do
		$RGBLINK -l $script -o $gbtemp $otemp > $outtemp 2>&1
		tryDiff ${script%.link}.out $outtemp
		rc=$(($? || $rc))
		ran_flag=1
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# The rest of the tests just links a file, and maybe checks the binary
	$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
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

# This test does its own thing
$RGBASM -o $otemp high-low/a.asm
$RGBLINK -o $gbtemp $otemp
$RGBASM -o $otemp high-low/b.asm
$RGBLINK -o $gbtemp2 $otemp
i="high-low.asm" tryDiff $gbtemp $gbtemp2
rc=$(($? || $rc))

rm -f $otemp $gbtemp $gbtemp2 $outtemp
exit $rc
