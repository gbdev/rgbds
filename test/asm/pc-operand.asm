SECTION "fixed", ROM0[0]

	rst @    ; rst 0
	ld de, @ ; ld de, 1
	bit @, h ; bit 4, h
	db @, @  ; db 6, 7

SECTION "floating", ROM0

	rst @    ; rst 8
	ld l, @  ; ld l, 9
	dw @, @  ; dw 11, 13
	dl @, @  ; dl 15, 19
