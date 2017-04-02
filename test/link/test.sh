otemp=$(mktemp)
gbtemp=$(mktemp)
outtemp=$(mktemp)

RGBASM=../../rgbasm
RGBLINK=../../rgblink

$RGBASM -o $otemp bank-numbers.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff bank-numbers.out $outtemp
head -c 20 $gbtemp > $otemp 2>&1
diff bank-numbers.out.bin $otemp
