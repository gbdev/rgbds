SECTION "Test", ROM0

_1234::

; without underscores
	dw _1234 ; label
	db 123, 123, 123 ; decimal
	dw 12345 ; decimal
	dw $abcd ; hex
	db &200 ; octal
	db %11110000, %10 ; binary
	dl 6.283185 ; fixed point

; with underscores
	dw _1234 ; label
	db 123, 1_23, 1__23 ; decimal
	dw 12_345 ; decimal
	dw $ab_cd ; hex
	db &2_0_0_ ; octal
	db %1111_0000, %1_0 ; binary
	dl 6_._283_185 ; fixed point
