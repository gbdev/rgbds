SECTION "sec", ROM0

X0 EQUS "0"

m: MACRO
\1 EQUS STRCAT("{X\2}", "+0")
ENDM

n = 0

REPT $7E
n1 = n + 1
NSTR EQUS STRSUB("{n}", 2, STRLEN("{n}") - 1)
N1STR EQUS STRSUB("{n1}", 2, STRLEN("{n1}") - 1)
XN1 EQUS STRCAT("X", "{N1STR}")
    m XN1, {NSTR}
    PURGE NSTR, N1STR, XN1
n = n + 1
ENDR

; string of 127 zeros separated by plus signs
X EQUS "{X7E}"

    db x+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+\
       X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X

x db 0
