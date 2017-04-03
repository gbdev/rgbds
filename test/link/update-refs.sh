otemp=$(mktemp)
gbtemp=$(mktemp)

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > bank-numbers.out 2>&1
head -c 20 $gbtemp > bank-numbers.out.bin 2>&1

$RGBASM -o $otemp wramx-contwram.asm
$RGBLINK -o $gbtemp $otemp > wramx-contwram-no-w.out 2>&1
$RGBLINK -w -o $gbtemp $otemp > wramx-contwram-w.out 2>&1

$RGBASM -o $otemp romx-tiny.asm
$RGBLINK -o $gbtemp $otemp > romx-tiny-no-t.out 2>&1
$RGBLINK -t -o $gbtemp $otemp > romx-tiny-t.out 2>&1

exit 0
