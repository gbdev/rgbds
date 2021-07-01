println 1 . println 1 + \ ; comment
1 . println 3 . println 4 ; comment

SECTION "test", ROM0

CallDE: push de . ret

Label1:: . Label2: . .local1: . .local2 . : . :

Copy: . : ld a, [hli] . ld [de], a . inc de . dec bc . jr nz, :- . ret

ld a, [hli] . ld h, [hl] . ld l, a

; please don't do this
	ld \
	b, \
	h . ld \
	c, \
	l . ret
