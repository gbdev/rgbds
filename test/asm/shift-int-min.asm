MACRO m
	println \1
	shift $8000_0000 ; INT32_MIN
	println \1
ENDM
	m 1, 2, 3
