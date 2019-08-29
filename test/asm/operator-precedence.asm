print: MACRO
	printv \1
	printt "\n"
ENDM

	print 1 == 1 || 1 == 2
	print (1 == 1) || (1 == 2)
