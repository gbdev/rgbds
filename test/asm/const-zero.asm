MACRO test_and
	if DEF(foo) && foo == 42
		println "Life, the Universe, and Everything!"
	else
		println "What do you get if you multiply six by seven?"
	endc
ENDM
	test_and
	DEF foo = 42
	test_and


MACRO test_or
	if DEF(DEBUG) || @ == $4567
		println "Here we are!"
	else
		println "Where are we?"
	endc
ENDM
	SECTION "Test OR", ROMX
	test_or ; Not constant
	DEF DEBUG EQU 1
	test_or


SECTION "Test arithmetic", ROM0
Floating:
	println Floating & 0
	println 0 & Floating
