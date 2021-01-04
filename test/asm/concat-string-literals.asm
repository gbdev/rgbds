	println "Hello," /* comment */ " " \
		"world""!"

two EQUS "\"2\""

	println "4^" two " = 16"

	println """multi-
line"""" """"con\ ; comment
cat""" " works"

makeprint: MACRO
\1 EQUS "println " \2
ENDM
	makeprint printfour, {two}"+"{two}
	printfour

	println "literal" STRCAT("expr") ; error
