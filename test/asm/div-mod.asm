_ASM equ 0

test: MACRO
; Test RGBASM
V equs "_ASM +"
	static_assert \#
	PURGE V
; Test RGBLINK
V equs "_LINK +"
	assert \#
	PURGE V
ENDM

for x, -300, 301
  for y, -x - 1, x + 2
    if y != 0
q = x / y
r = x % y
      test (V (q * y + r)) == (V x)
      test (V (x + y) % y) == (V r)
      test (V (x - y) % y) == (V r)
    endc
  endr
endr

for x, -300, 301
  for p, 31
y = 2 ** p
r = x % y
m = x & (y - 1)
    test (V r) == (V m)
  endr
endr

SECTION "LINK", ROM0
_LINK::
