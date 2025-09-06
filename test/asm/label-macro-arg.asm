MACRO m1
	def x\1
ENDM

DEF S EQUS "y"
DEF S2 EQUS "yy"

MACRO m2
	def {S}\1
ENDM

	m1 = 5
	m2 = 6
	m1 x = 7
	m2 2 = 8

	println x
	println y
	println xx
	println y2


MACRO test_char
DEF VAR_DEF equs "DEF sizeof_\1something = 0"
{VAR_DEF}
DEF sizeof_\1something = 1
	PURGE VAR_DEF

DEF VAR_PRINT equs "println \"sizeof_\1something equals {sizeof_\1something}\""
	{VAR_PRINT}
	PURGE VAR_PRINT
ENDM

	test_char _
	test_char @
	test_char #

	test_char .
	test_char :
