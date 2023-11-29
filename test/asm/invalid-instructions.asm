SECTION "invalid", ROM0[$10000]
	ld [hl], [hl]
	ld a, [$00ff+c]
	ld b, [c]
	ld b, [bc]
	ld b, [$4000]
	bit 8, a
	rst $40
