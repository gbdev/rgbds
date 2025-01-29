MACRO mac
	println "\1"
	println \1
ENDM
mac "hello \\\"\t\r\0\n\ ; comment
 \wor\
ld"
mac """goodbye
cruel\	; comment
\nworld"""
mac "\