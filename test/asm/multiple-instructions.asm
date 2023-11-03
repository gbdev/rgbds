SECTION "test", ROM0

push hl :: pop hl :: ret

Label: nop :: call z, .local :: ld b, a
.local push bc :: jr z, Label :: pop bc
	nop :: ld a, \
	b :: ret

Label2::jr Label2::ret
.local2::call nz, .local2::ret
