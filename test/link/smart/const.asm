
BASE equ 1

SECTION "root", ROM0[0]

	db BASE

; Although the base address is specified using a referenced symbol, no symbol within the section is
SECTION "A", ROM0[BASE]

Label:
	db 2, Label

SECTION "B", ROM0[3]

Unrefd:
	db Unrefd
