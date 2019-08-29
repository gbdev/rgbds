print: MACRO
	printv \1
	printt "\n"
ENDM


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

	print x
	print y
	print xx
	print yy


test_char: MACRO
VAR_DEF equs "sizeof_\1something = 0"
VAR_DEF
sizeof_\1something = 1
	PURGE VAR_DEF

VAR_PRINT equs "printt \"sizeof_\1something equals {sizeof_\1something}\\n\""
	VAR_PRINT
	PURGE VAR_PRINT
ENDM

	test_char _
	test_char @
	test_char #
	test_char .

	test_char :
