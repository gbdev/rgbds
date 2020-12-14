print: MACRO
	PRINTT "\1"
	PRINTT "\n"
ENDM

	print John "Danger" Smith
	print \\A\nB
	print C\
D
	print E\!F ; illegal character escape


iprint: MACRO
	PRINTT "{\1}"
	PRINTT "\n"
ENDM

s EQUS "hello"
	iprint s
