SECTION "test", ROM0

Global: dw Global
.ret dw .ret
.xor: dw .xor
Global.section: dw .section

DEF n EQU 42
.n: dw .n

MACRO foo
ENDM
.foo: dw Global.foo
