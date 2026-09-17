; XXX: This test is brittle, since it relies on more than just the scrambling algorithm.
; For example, if the order in which sections are processed changes,
; then they will also be scrambled differently, and this test will fail.
; As long as the actual values are coherent, feel free to change the assertions.

DEF N equ 6

assert BANK(xLabel5) == 3
assert BANK(xLabel4) == 2
assert BANK(xLabel3) == 1
assert BANK(xLabel2) == 3
assert BANK(xLabel1) == 2
assert BANK(xLabel0) == 1

assert BANK(sLabel5) == 3
assert BANK(sLabel4) == 2
assert BANK(sLabel3) == 1
assert BANK(sLabel2) == 0
assert BANK(sLabel1) == 3
assert BANK(sLabel0) == 2

assert BANK(wLabel5) == 4
assert BANK(wLabel4) == 3
assert BANK(wLabel3) == 2
assert BANK(wLabel2) == 1
assert BANK(wLabel1) == 4
assert BANK(wLabel0) == 3

FOR i, N
	SECTION "floating{d:i}", ROMX
	xLabel{d:i}:: ds $2000, i

	SECTION "sram{d:i}", SRAM
	sLabel{d:i}:: dw

	SECTION "wram{d:i}", WRAMX
	wLabel{d:i}:: dw
ENDR
