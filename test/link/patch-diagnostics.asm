def fzero equs "startof(\"test\")"
section "test", rom0
ld a, $8000_0000 / ({fzero} - 1)
ld a, $8000_0000 / ({fzero} - 2)
ld a, 1 << ({fzero} - 1)
ld a, 1 << ({fzero} + 32)
ld a, ({fzero} - 1) >> 1
ld a, 1 >> ({fzero} - 1)
ld a, 1 >> ({fzero} + 32)
ld a, 1 >>> ({fzero} - 1)
ld a, 1 >>> ({fzero} + 32)

; We rely on this landing at address $0000, which isn't *guaranteed*...
assert startof("test") == 0
