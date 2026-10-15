def X equ 0

MACRO test
; Test RGBASM
	def v equs "X +"
	static_assert \#
	purge v
; Test RGBLINK
	def v equs "Y +"
	assert \#
	purge v
ENDM

	test (v 2)*(v 10)**(v 2)*(v 2) == (v 400)
	test -(v 3)**(v 4) == (v -81)
	test (v 1) << (v 30) == (v $4000_0000)
	test (v 2)**(v 30) == (v $4000_0000)
	test (v 37)/(v 2) == (v 18)

	static_assert DIV(5.0, 2.0) == 2.5
	static_assert DIV(-5.0, 2.0) == -2.5
	static_assert DIV(5.0, 0.0) == $7fff_ffff ; +inf => INT32_MAX
	static_assert DIV(-5.0, 0.0) == $8000_0000 ; -inf => INT32_MIN
	static_assert DIV(0.0, 0.0) == $0000_0000 ; nan => 0

	static_assert MUL(10.0, 0.5) == 5.0
	static_assert MUL(10.0, 0.0) == 0.0

	static_assert FMOD(5.0, 2.0) == 1.0
	static_assert FMOD(-5.0, 2.0) == -1.0
	static_assert FMOD(5.0, 0.0) == 0 ; nan
	static_assert FMOD(-5.0, 0.0) == 0 ; nan
	static_assert FMOD(0.0, 0.0) == 0 ; nan

	static_assert POW(10.0, 2.0) == 100.0
	static_assert POW(100.0, 0.5) == 10.0

	static_assert LOG(100.0, 10.0) == 2.0
	static_assert LOG(256.0, 2.0) == 8.0
	static_assert LOG(10.0, 1.0) == $7fff_ffff ; +inf
	static_assert LOG(0.0, 2.71828) == $8000_0000 ; -inf
	static_assert LOG(-1.0, 2.71828) == 0 ; nan

	static_assert ROUND(1.5) == 2.0
	static_assert ROUND(-1.5) == -2.0

	static_assert CEIL(1.5) == 2.0
	static_assert CEIL(-1.5) == -1.0

	static_assert FLOOR(1.5) == 1.0
	static_assert FLOOR(-1.5) == -2.0

SECTION "Y", ROM0
Y::
