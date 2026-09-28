SECTION "limit", ROM0, ALIGN[8, 128]
Limit:
add sp, Limit
add sp, -Limit
ld hl, sp + Limit
ld hl, sp - Limit

SECTION "test", ROMX
Invalid:
add sp, Invalid
add sp, -Invalid
ld hl, sp + Invalid
ld hl, sp - Invalid
