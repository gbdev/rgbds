SECTION "test1", ROM0
align 4           ; PC = $xxx0
dw $0123
align 4, 2        ; PC = $xxx2
ds align [8, $C2] ; PC = $xxC2 (ds 0)
align 8, $C2      ; PC = $xxC2
dw $4567

SECTION "test2", ROM0[$01C0]
align 4           ; PC = $01C0
dw $89ab
align 4, 2        ; PC = $01C2
ds align [8, $C2] ; PC = $01C2 (ds 0)
align 8, $C2      ; PC = $01C2
dw $cdef
