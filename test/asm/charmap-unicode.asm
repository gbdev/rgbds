MACRO test
	charmap \1, 42
	assert charlen(\1) == 1
	assert incharmap(\1) == 1
ENDM

	test "a" ; one ASCII
	test "hi~!!1" ; many ASCII
	test "デ" ; one UTF-8
	test "グレイシア" ; many UTF-8
	test "Pokémon" ; mixed
