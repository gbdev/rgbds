section "tilemap", rom0
/*
input:
$20
$01 $03 $05 $07 $09
$02 $04 $06 $08 $10
$00 $de $01 $df $80
5
*/
def tilemap equs readfile("readfile-binary.inc.bin")
def area = bytelen(#tilemap) - 2
def offset = strbyte(#tilemap, 0)
def width = strbyte(#tilemap, area + 1)
db width, area / width
for idx, area
  db strbyte(#tilemap, idx + 1) + offset
endr
/*
output:
5, 3
$21 $23 $25 $27 $29
$22 $24 $26 $28 $30
$20 $fe $21 $ff $a0
*/
