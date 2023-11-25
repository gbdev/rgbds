SECTION "good", ROM0, ALIGN[4, -2]
align 4, 14 ; -2 == (1 << 4) - 2 == -2 mod (1 << 4)

SECTION "bad+", ROM0, ALIGN[4, 18] ; out of range

SECTION "bad-", ROM0, ALIGN[4, -20] ; out of range negative
