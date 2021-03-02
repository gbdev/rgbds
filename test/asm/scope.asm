SECTION "test", ROM0

	assert !STRLEN("{__SCOPE__}")

SomeFunction:
	println __FILE__, ":{d:__LINE__}:{__SCOPE__}: 2 + 2 = ", 2+2

verify: MACRO
	assert !STRCMP("{__SCOPE__}", "\1")
	assert __SCOPE__ == \1
ENDM

Alpha:
	verify Alpha
.local
	verify Alpha

Beta::
.local::
	verify Beta

S EQUS """
Gamma:
	verify Gamma
"""
	S
	PURGE S
	verify Gamma
