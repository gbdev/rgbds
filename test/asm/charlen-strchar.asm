	opt Wno-unmapped-char
	charmap "<NULL>", $00
	charmap "A", $10
	charmap "B", $20
	charmap "C", $30
	charmap "Bold", $88

SECTION "test", ROM0

DEF S EQUS "XBold<NULL>ABC"

	static_assert CHARLEN("{S}") == 6
	println STRCHAR("{S}", 1)

	static_assert !STRCMP(STRCHAR("{S}", 1), "Bold")
	static_assert STRCHAR("{S}", -5) == STRCHAR("{S}", CHARLEN("{S}") - 5)
	static_assert STRCHAR("{S}", 1) == "Bold" && "Bold" == $88
	static_assert STRCHAR("{S}", 0) == $58 ; ASCII "X"
	db "{S}"

	newcharmap ascii

	static_assert CHARLEN("{S}") == 14
	println STRCHAR("{S}", 1)
	static_assert !STRCMP(STRCHAR("{S}", 1), "B")
	static_assert STRCHAR("{S}", -5) == STRCHAR("{S}", CHARLEN("{S}") - 5)
	static_assert STRCHAR("{S}", 1) == "B" && "B" == $42 ; ASCII "B"
	static_assert STRCHAR("{S}", 0) == $58 ; ASCII "X"
	db "{S}"
