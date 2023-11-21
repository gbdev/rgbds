SECTION "aligned", ROM0, ALIGN[8]
db 1, 2, 3, 4, 5 ; five
ds align[8, $ff], 6 ; (1 << 8) - five - one = 250 bytes
db 7 ; one
align 8
assert @ - STARTOF("aligned") == 5 + 250 + 1

SECTION "fixed", ROM0[$100]
ds align[2] ; already aligned, 0 bytes
db 8, 9, 10 ; three
ds align[4, $e], 11 ; (1 << 4) - three - two = 11 bytes
db 12, 13 ; two
align 4
assert @ - STARTOF("fixed") == 3 + 11 + 2

SECTION "floating", ROM0
db 14, 15 ; two
ds align[4] ; not aligned, 0 bytes
align 4 ; redundant
db 16, 17, 18 ; three
align 4, 3
ds align[5], 19, 20 ; (1 << 4) - three = 13 bytes
db 21 ; one
align 5, 1
assert @ - STARTOF("floating") == 2 + 3 + 13 + 1
