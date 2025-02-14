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

MACRO xstrsub
	PRINTLN "STRSUB(\#): ", STRSUB(\#)
ENDM

	xstrsub "ABC", 1, 1
	xstrsub "ABC", 2, 1
	xstrsub "ABC", 3, 1
	xstrsub "ABC", -3, 1
	xstrsub "ABC", -2, 1
	xstrsub "ABC", -1, 1
	xstrsub "ABC", 2
	xstrsub "ABC", 0
	xstrsub "ABC", -2
	xstrsub "ABC", 5
	xstrsub "ABC", -5
	xstrsub "ABC", 1, 2
	xstrsub "ABC", 2, 2
	xstrsub "ABC", 2, 32
	xstrsub "ABC", 2, 300
	xstrsub "ABC", -4, 300
	xstrsub "ABC", 4, 0
	xstrsub "ABC", 5, 0
	xstrsub "ABC", 4, 1
	xstrsub "カタカナ", 1, 2
	xstrsub "カタカナ", 3, 2
	xstrsub "カタカナ", 3, 10
	xstrsub "g̈", 1, 1
	xstrsub "g̈", 1, 2
