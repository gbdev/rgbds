MACRO mac
	println "outer mac: \@ (\#)"
	INCLUDE "include-unique-id.inc"
ENDM

	DEF state = 1
	mac hello, world
	mac goodbye, world

REPT 2
	DEF state = 2
	println "outer rept before: \@"
	INCLUDE "include-unique-id.inc"

	FOR n, 3
		DEF state = 3
		println "outer for: \@ ({n})"
		INCLUDE "include-unique-id.inc"
	ENDR

	DEF state = 4
	println "outer rept after: \@"
	INCLUDE "include-unique-id.inc"
ENDR