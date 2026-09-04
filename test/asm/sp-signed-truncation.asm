section "test", rom0

opt Wtruncation=1

; good
ld hl, sp + 0
ld hl, sp + 127
ld hl, sp - 128
ld hl, sp + -128
add sp, 0
add sp, 127
add sp, -128

; bad
ld hl, sp + 128
ld hl, sp - 129
ld hl, sp + 255
ld hl, sp - 256
add sp, 128
add sp, -129
add sp, 255
add sp, -256
