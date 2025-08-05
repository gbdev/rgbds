MACRO xstrslice
	PRINTLN "STRSLICE(\#): ", STRSLICE(\#)
ENDM

	xstrslice "ABC", 0, 1
	xstrslice "ABC", 1, 2
	xstrslice "ABC", 2, 3
	xstrslice "ABC", -3, -2
	xstrslice "ABC", -2, -1
	xstrslice "ABC", -1, -0 ; lol
	xstrslice "ABC", -1, 3
	xstrslice "ABC", 1
	xstrslice "ABC", -2
	xstrslice "ABC", 4
	xstrslice "ABC", -4
	xstrslice "ABC", 0, 2
	xstrslice "ABC", 1, 3
	xstrslice "ABC", 1, 31
	xstrslice "ABC", 1, 300
	xstrslice "ABC", -4, 300
	xstrslice "ABC", 3, 3
	xstrslice "ABC", 4, 4
	xstrslice "ABC", 3, 4
	xstrslice "カタカナ", 0, 2
	xstrslice "カタカナ", 2, 4
	xstrslice "カタカナ", 2, 12
	xstrslice "g̈", 0, 1
	xstrslice "g̈", 0, 2
