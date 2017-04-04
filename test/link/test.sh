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

$RGBASM -o $otemp wramx-contwram.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff wramx-contwram-no-w.out $outtemp
$RGBLINK -w -o $gbtemp $otemp > $outtemp 2>&1
diff wramx-contwram-w.out $outtemp

$RGBASM -o $otemp romx-tiny.asm
$RGBLINK -o $gbtemp $otemp > $outtemp 2>&1
diff romx-tiny-no-t.out $outtemp
$RGBLINK -t -o $gbtemp $otemp > $outtemp 2>&1
diff romx-tiny-t.out $outtemp

exit 0
