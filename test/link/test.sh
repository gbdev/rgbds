#!/bin/sh
otemp=$(mktemp)
gbtemp=$(mktemp)
gbtemp2=$(mktemp)
outtemp=$(mktemp)
rc=0

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff bank-numbers.out $outtemp
rc=$(($? || $rc))
head -c 20 $gbtemp > $otemp 2>&1
diff bank-numbers.out.bin $otemp
rc=$(($? || $rc))

$RGBASM -o $otemp wramx-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff wramx-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff wramx-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp vram-fixed-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff vram-fixed-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff vram-fixed-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp vram-floating-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff vram-floating-dmg-mode-no-d.out $outtemp
rc=$(($? || $rc))
$RGBLINK -d -o $gbtemp $otemp > $outtemp 2>&1
diff vram-floating-dmg-mode-d.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp romx-tiny.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff romx-tiny-no-t.out $outtemp
rc=$(($? || $rc))
$RGBLINK -t -o $gbtemp $otemp > $outtemp 2>&1
diff romx-tiny-t.out $outtemp
rc=$(($? || $rc))

$RGBASM -o $otemp high-low-a.asm
$RGBLINK -o $gbtemp $otemp
$RGBASM -o $otemp high-low-b.asm
$RGBLINK -o $gbtemp2 $otemp
diff $gbtemp $gbtemp2
rc=$(($? || $rc))

exit $rc
