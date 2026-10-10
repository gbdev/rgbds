SECTION "beta", ROM0
Beta::
	db 4, 5, 6
End:

SECTION "b", WRAM0
wBeta::
	ds 3
.End::

SECTION UNION "U", WRAM0
wStart:
	.long1: dl
wEnd:

; Reference all the labels so they get output
; even though they're not all exported.
assert Beta >= 0
assert End >= 0
assert wBeta >= 0
assert wBeta.End >= 0
assert wStart >= 0
assert wStart.long1 >= 0
assert wEnd >= 0
