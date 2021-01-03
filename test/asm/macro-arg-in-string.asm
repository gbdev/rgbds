print1: MACRO
	PRINTLN "\1"
ENDM

	print1 John "Danger" Smith
	print1 \\A\nB
	print1 C\
D
	print1 E\!F ; illegal character escape


iprint: MACRO
	PRINTLN "{\1}"
ENDM

s EQUS "hello"
	iprint s

symprint: MACRO
	PRINTLN {\1}
ENDM

hello EQUS "\"goodbye\""
	symprint s
