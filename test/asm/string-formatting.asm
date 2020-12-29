n equ 300
m equ -42
f equ -123.0456
s equs "hello"

	printt "<{ -6d:n}> <{+06u:n}> <{5x:n}> <{#16b:n}>\n"
	printt "<{u:m}> <{+3d:m}> <{#016o:m}>\n"
	printt "<{f:_PI}> <{06f:f}> <{.10f:f}>\n"
	printt "<{#-10s:s}> <{10s:s}>\n"

foo: macro
	printt "<{\1}>\n"
endm

	foo  -6d:n ; space is trimmed
