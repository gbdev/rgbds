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

	println x
	println y
	println xx
	println yy


test_char: MACRO
VAR_DEF equs "sizeof_\1something = 0"
VAR_DEF
sizeof_\1something = 1
	PURGE VAR_DEF

VAR_PRINT equs "println \"sizeof_\1something equals {sizeof_\1something}\""
	VAR_PRINT
	PURGE VAR_PRINT
ENDM

	test_char _
	test_char @
	test_char #

	test_char .
	test_char :
