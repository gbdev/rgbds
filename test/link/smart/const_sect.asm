
SECTION "root", ROM0[0]

BASE equ 1 ; This may be defined while section "root" is in scope, it does not belong to the section

	db BASE

; Although the base address is specified using a referenced symbol, no symbol within the section is
SECTION "A", ROM0[BASE]

Label:
	db 2, Label

SECTION "B", ROM0[3]

Unrefd:
	db Unrefd
