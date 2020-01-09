#!/bin/sh
export LC_ALL=C

otemp=$(mktemp)
gbtemp=$(mktemp)
gbtemp2=$(mktemp)
outtemp=$(mktemp)
rc=0

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr bank-numbers.out $outtemp
rc=$(($? || $rc))
dd if=$gbtemp count=1 bs=20 > $otemp 2>/dev/null
diff --strip-trailing-cr bank-numbers.out.bin $otemp
rc=$(($? || $rc))

$RGBASM -o $otemp section-attributes.asm
$RGBLINK -l section-attributes.link -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr section-attributes.out $outtemp
rc=$(($? || $rc))
$RGBLINK -l section-attributes-mismatch.link -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr section-attributes-mismatch.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp wramx-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr wramx-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr wramx-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp vram-fixed-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr vram-fixed-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr vram-fixed-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp vram-floating-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr vram-floating-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr vram-floating-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp romx-tiny.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr romx-tiny-no-t.out $outtemp
rc=$(($? || $rc))
$RGBLINK -t -o $gbtemp $otemp > $outtemp 2>&1
diff --strip-trailing-cr romx-tiny-t.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp high-low-a.asm
$RGBLINK -o $gbtemp $otemp
$RGBASM -o $otemp high-low-b.asm
$RGBLINK -o $gbtemp2 $otemp
diff --strip-trailing-cr $gbtemp $gbtemp2
rc=$(($? || $rc))

$RGBASM -o $otemp all-instructions.asm
$RGBLINK -o $gbtemp $otemp
diff --strip-trailing-cr all-instructions.out.bin $gbtemp
rc=$(($? || $rc))

rm -f $otemp $gbtemp $gbtemp2 $outtemp
exit $rc
