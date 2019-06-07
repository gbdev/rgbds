SECTION "sec", ROM0

xstrsub: MACRO
	PRINTT STRSUB(\1, \2, \3)
	PRINTT "\n"
ENDM

	xstrsub "ABC", 1, 1
	xstrsub "ABC", 2, 1
	xstrsub "ABC", 3, 1
	xstrsub "ABC", 1, 2
	xstrsub "ABC", 2, 2
	xstrsub "ABC", 2, 32
	xstrsub "ABC", 2, 300
	xstrsub "ABC", 0, 300
	xstrsub "ABC", 4, 0
	xstrsub "ABC", 4, 1
	xstrsub "カタカナ", 1, 2
	xstrsub "カタカナ", 3, 2
	xstrsub "カタカナ", 3, 10
	xstrsub "g̈", 1, 1
	xstrsub "g̈", 1, 2
