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

# CONVENTION: please always call these with the reference as the first arg
tryDiff () {
	diff -u --strip-trailing-cr $1 $2 || (echo "${bold}${red}$3 mismatch!${rescolors}${resbold}"; false)
}
tryCmp () {
	cmp $1 $2 || (../../contrib/gbdiff.bash $1 $2; echo "${bold}${red}$3 mismatch!${rescolors}${resbold}"; false)
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
			tryDiff ${i%.asm}-no${flag}.out $outtemp  ${i%.asm}-no${flag}.out
			rc=$(($? || $rc))
			ran_flag=1
		fi
		if [ -f ${i%.asm}${flag}.out ]; then
			$RGBLINK ${flag} -o $gbtemp $otemp > $outtemp 2>&1
			tryDiff ${i%.asm}${flag}.out $outtemp  ${i%.asm}${flag}.out
			rc=$(($? || $rc))
			ran_flag=1
		fi
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# Other tests have several linker scripts
	find . -name "${i%.asm}*.link" | while read script; do
		$RGBLINK -l $script -o $gbtemp $otemp > $outtemp 2>&1
		tryDiff ${script%.link}.out $outtemp  ${script%.link}.out
		rc=$(($? || $rc))
		ran_flag=1
	done
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# The rest of the tests just links a file, and maybe checks the binary
	$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
	if [ -f ${i%.asm}.out ]; then
		tryDiff ${i%.asm}.out $outtemp  ${i%.asm}.out
		rc=$(($? || $rc))
	fi

	bin=${i%.asm}.out.bin
	if [ -f $bin ]; then
		dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < $bin)) > $otemp 2>/dev/null
		tryCmp $bin $otemp  $bin
		rc=$(($? || $rc))

		# TODO: check for output to stdout
	fi

	# TODO: check for RGBASM piping into RGBLINK
done

ffpad() { # Pads $1 to size $3 into $2 with 0xFF bytes
	dd if=/dev/zero count=1 ibs=$3 2>/dev/null | tr '\000' '\377' > $2
	dd if="$1" of=$2 conv=notrunc 2>/dev/null
}
for i in smart/*.asm; do
	$RGBASM -o $otemp $i
	$RGBLINK -p 0xff -o $gbtemp $otemp
	SIZE=$(wc -c $gbtemp | cut -d ' ' -f 1)
	ffpad "${i%.asm}.bin" $gbtemp2 $SIZE
	tryCmp $gbtemp2 $gbtemp  "${i%.asm}.bin"
	rc=$(($? || $rc))
	$RGBLINK -p 0xff -vs "root" -o $gbtemp $otemp
	ffpad "${i%.asm}.smart.bin" $gbtemp2 $SIZE
	tryCmp $gbtemp2 $gbtemp  "${i%.asm}.smart.bin"
	rc=$(($? || $rc))
done

# These tests do their own thing

$RGBASM -o $otemp high-low/a.asm
$RGBLINK -o $gbtemp $otemp
$RGBASM -o $otemp high-low/b.asm
$RGBLINK -o $gbtemp2 $otemp
tryCmp $gbtemp $gbtemp2  "high-low/<...>.asm"
rc=$(($? || $rc))

$RGBASM -o $otemp bank-const/a.asm
$RGBASM -o $gbtemp2 bank-const/b.asm
$RGBLINK -o $gbtemp $gbtemp2 $otemp > $outtemp 2>&1
tryDiff bank-const/err.out $outtemp  "bank-const/<...>.asm"
rc=$(($? || $rc))

$RGBASM -o $otemp section-union/good/a.asm
$RGBASM -o $gbtemp2 section-union/good/b.asm
$RGBLINK -o $gbtemp -l section-union/good/script.link $otemp $gbtemp2
dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < section-union/good/ref.out.bin)) > $otemp 2>/dev/null
tryCmp section-union/good/ref.out.bin $otemp  "section-union/good/<...>.asm"
rc=$(($? || $rc))
$RGBASM -o $otemp section-union/fragments/a.asm
$RGBASM -o $gbtemp2 section-union/fragments/b.asm
$RGBLINK -o $gbtemp $otemp $gbtemp2
dd if=$gbtemp count=1 bs=$(printf %s $(wc -c < section-union/fragments/ref.out.bin)) > $otemp 2>/dev/null
tryCmp section-union/fragments/ref.out.bin $otemp  "section-union/fragments/<...>.asm"
rc=$(($? || $rc))
for i in section-union/*.asm; do
	$RGBASM -o $otemp   $i
	$RGBASM -o $gbtemp2 $i -DSECOND
	if $RGBLINK $otemp $gbtemp2 > $outtemp 2>&1; then
		echo "${bold}${red}$i didn't fail to link!${rescolors}${resbold}"
		rc=1
	fi
	echo --- >> $outtemp
	# Ensure RGBASM also errors out
	echo 'SECOND equs "1"' | cat $i - $i | $RGBASM - 2>> $outtemp
	tryDiff ${i%.asm}.out $outtemp  ${i%.asm}.out
	rc=$(($? || $rc))
done

rm -f $otemp $gbtemp $gbtemp2 $outtemp
exit $rc
