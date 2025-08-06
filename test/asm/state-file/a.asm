OPT Wno-unmapped-char

DEF constant EQU 1
DEF variable = 2
DEF string EQUS "hello!"
CHARMAP "c", 4
MACRO polo
	db 5
ENDM

def variable += 1

def con2 equ -1
def var2 = variable**2
def str2 equs strcat("{string}", "\0\n\t\r\\\"\{")
charmap "c2", 10, -11, 987654321

PURGE polo
MACRO mac2
	!?@#;^&
ENDM

newcharmap map2, main
charmap "\0\n\t\r", '\t', '\r', '\0', '\n'

REDEF string EQUS "goodbye~"
