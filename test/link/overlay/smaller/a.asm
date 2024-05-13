SECTION "start", ROM0[0]
db 1, 2, 3, 4
ds 4
db 5, 6, 7, 8

SECTION "end", ROMX[$4000], BANK[2]
db 9, 10, 11, 12
ds 4
db 13, 14, 15, 16
