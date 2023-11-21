SECTION "test", ROM0
align 3 ; 3 < 4, so this is used in `ds align` below
db 1, 2, 5 ; three
ds align[4] ; (1 << 3) - three = 5 bytes
db 10, 20 ; two
align 4, 2
assert @ - STARTOF("test") == 3 + 5 + 2
