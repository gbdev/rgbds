m1: MACRO
x\1
ENDM

S EQUS "y"
S2 EQUS "yy"

m2: MACRO
S\1
ENDM

print: MACRO
	printv \1
	printt "\n"
ENDM

	m1 = 5
	m2 = 6
	m1 x = 7
	m2 2 = 8
	m1 a.test = 9

	print x
	print y
	print xx
	print yy
	print xa.test
