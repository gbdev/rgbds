#!/bin/sh
otemp=$(mktemp)
gbtemp=$(mktemp)

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > bank-numbers.out 2>&1
dd if=$gbtemp count=1 bs=20 > bank-numbers.out.bin 2>/dev/null

$RGBASM -o $otemp section-attributes.asm
$RGBLINK -l section-attributes.link \
	-o $gbtemp $otemp > section-attributes.out 2>&1
$RGBLINK -l section-attributes-mismatch.link \
	-o $gbtemp $otemp > section-attributes-mismatch.out 2>&1

$RGBASM -o $otemp wramx-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > wramx-dmg-mode-no-d.out 2>&1
$RGBLINK -d -o $gbtemp $otemp > wramx-dmg-mode-d.out 2>&1

$RGBASM -o $otemp vram-fixed-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > vram-fixed-dmg-mode-no-d.out 2>&1
$RGBLINK -d -o $gbtemp $otemp > vram-fixed-dmg-mode-d.out 2>&1

$RGBASM -o $otemp vram-floating-dmg-mode.asm
$RGBLINK -o $gbtemp $otemp > vram-floating-dmg-mode-no-d.out 2>&1
$RGBLINK -d -o $gbtemp $otemp > vram-floating-dmg-mode-d.out 2>&1

$RGBASM -o $otemp romx-tiny.asm
$RGBLINK -o $gbtemp $otemp > romx-tiny-no-t.out 2>&1
$RGBLINK -t -o $gbtemp $otemp > romx-tiny-t.out 2>&1

$RGBASM -o $otemp all-instructions.asm
$RGBLINK -o all-instructions.out.bin $otemp 2>&1

rm -f $otemp $gbtemp
exit 0
