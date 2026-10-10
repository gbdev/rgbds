SECTION "alpha", ROM0
Alpha::
	db 1, 2, 3
End:

SECTION "a", WRAM0
wAlpha::
	ds 3
.End::

SECTION UNION "U", WRAM0
wStart:
	.word1: dw
	.word2: dw
wEnd:

; Reference all the labels so they get output
; even though they're not all exported.
assert Alpha >= 0
assert End >= 0
assert wAlpha >= 0
assert wAlpha.End >= 0
assert wStart >= 0
assert wStart.word1 >= 0
assert wStart.word2 >= 0
assert wEnd >= 0
