SECTION "invalid", ROM0[$10000]
	ld [hl], [hl]
	ld a, [$00ff+c]
	ld b, [c]
	ld b, [bc]
	ld b, [$4000]
	bit 8, a
	rst $40

	ld bc, bc
	ld de, hl
	ld hl, de

	ld hl, sp ; no offset!
	; ld sp, hl is valid

	ld sp, bc
	ld bc, sp

	ld af, bc
	ld bc, af
