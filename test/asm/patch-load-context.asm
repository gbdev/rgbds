; SPDX-License-Identifier: MIT

; Assertions may be emitted without a current section.
assert End == 23

; Start the LOAD fragment after existing RAM data, ahead of the ROM offset.
SECTION FRAGMENT "ram", WRAMX
	ds 3

SECTION "rom", ROM0[0]
	db $a5, $5a
LOAD FRAGMENT "ram", WRAMX
	; The floating LOAD address and bank must be used for every patch type.
	db BANK(@)             ; 1
	db LOW(@)              ; 4
	dw @                   ; $d005
	dl @                   ; $0000d007
	ld a, LOW(@)           ; ld a, 11 (PC before the opcode)
	ld hl, @               ; ld hl, $d00d
	ds 3, LOW(@), BANK(@)   ; 16, 1, 16 (PC before any DS bytes)
	jr Target              ; jr +0
Target:
	assert @ == $d015
	assert BANK(@) == 1

	; Restoring the section must restore both the output and symbol locations.
	PUSHS
	SECTION "other", WRAM0
		ds 7
	POPS
	dw @                   ; $d015
ENDL
	db LOW(@)              ; 22 (back in ROM)
End:
ENDSECTION
assert End == 23
