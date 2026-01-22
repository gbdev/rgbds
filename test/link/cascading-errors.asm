SECTION "zero", ROM0[$0]
Zero:

; Pin the section such that a jr to 0 is out of range
SECTION "test", ROM0[$1000]
	;; the fallback value for an undefined symbol used to be its index in the object file,
	;; but is now zero as of RGBLINK v1.0.1
	dw Bar
	dw Foo / Bar
	dw Foo / Zero

	rst Foo

	jr NonExist

	ldh a, [hNonExist + $200]

	assert Foo == 42
	assert WARN, Bar == 42
