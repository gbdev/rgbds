m1: MACRO
x\1
ENDM

S EQUS "y"
S2 EQUS "yy"

m2: MACRO
S\1
ENDM

	m1 = 5
	m2 = 6
	m1 x = 7
	m2 2 = 8

	printv x
	printt "\n"

	printv y
	printt "\n"

	printv xx
	printt "\n"

	printv yy
	printt "\n"
