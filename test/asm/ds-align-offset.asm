SECTION "test", ROM0
align 3, 3 ; 3 < 4, so this is used in `ds align` below
db 2, 22, 222 ; three
ds align[4, 4] ; (1 << 3) - 3 - three + 4 = 6 bytes
db 42 ; one
align 4, 5
assert @ - STARTOF("test") == 3 + 6 + 1
