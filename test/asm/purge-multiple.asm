SECTION "s", ROM0
u::
def v = 0
def w equ 1
def x equs "2"
MACRO y
ENDM
; purge many symbols at once to test parser reallocation
PURGE u, v, w, x, y
