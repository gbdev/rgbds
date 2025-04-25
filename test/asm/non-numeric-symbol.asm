MACRO mac
ENDM

DEF n EQU mac
DEF v = 2 + mac
DEF k RB mac * 2

SECTION "test", ROM0
db mac
dw 2 + mac
dl mac * 2
