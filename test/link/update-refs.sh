otemp=$(mktemp)
gbtemp=$(mktemp)

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > bank-numbers.out 2>&1
head -c 20 $gbtemp > bank-numbers.out.bin 2>&1
