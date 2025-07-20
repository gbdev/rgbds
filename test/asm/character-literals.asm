def s equs "d"

charmap "A", 1
charmap "B", 2
charmap "c{s}e", 3
charmap "F", 4, 5, 6
charmap "'", 42
charmap "\"", 1234
charmap "\n\r\t\0", 1337
charmap "',\",\\", 99

MACRO char
	assert (\1) == (\2)
ENDM

char 'A', 1
char 'B', 2
char 'c{s}e', 3
char '\'', 42
char '"', 1234
char '\n\r\t\0', 1337
char '\',",\\', 99

char charval("c{s}e", 0), 'c{s}e'

def v equs "\n\r\t\0"
def x = '{v}'
char x, '\n\r\t\0'

; errors
char '?', $3f ; ASCII
char 'F', 0
char 'ABF', 0
char '\n\r\t', 0
assert 0 == '
