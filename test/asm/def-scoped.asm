SECTION "test", ROM0

; this is okay...
Label:
Label.local:

; ...but these are not

DEF n EQU 1
DEF n.local EQU 2

DEF x = 1
DEF x.local = 2

DEF s EQUS "..."
DEF s.local EQUS "..."
