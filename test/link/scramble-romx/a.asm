SECTION "fixed", ROMX, BANK[3]
db BANK(xLabel1), BANK(xLabel2), BANK(xLabel3), BANK(wLabel), BANK(sLabel)
ds $1000 - 5, 4

SECTION "floating1", ROMX
xLabel1:: ds $3000, 1

SECTION "floating2", ROMX
xLabel2:: ds $3000, 2

SECTION "floating3", ROMX
xLabel3:: ds $3000, 3

SECTION "wram", WRAMX
wLabel:: ds 2

SECTION "sram", SRAM
sLabel:: ds 2
