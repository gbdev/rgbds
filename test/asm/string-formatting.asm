def n equ 300
def m equ -42
def f equ -123.0456
def pi equ 3.14159
def s equs "hello"
def t equs "\"\\t\" is '\t'"
def u equs "\t\r\0\n"

	println "<{ -6d:n}> <{+06u:n}> <{5x:n}> <{#16b:n}>"
	println "<{u:m}> <{+3d:m}> <{#016o:m}>"
	println "<{f:pi}> <{06.f:f}> <{.10f:f}>"
	println "\"{#-20s:t}\", \"{#20s:t}\", \"{20s:t}\""
	println "{#s:u}"

macro foo
	println "\1 <{\1}>"
endm

	foo  -6d:n ; space is trimmed
