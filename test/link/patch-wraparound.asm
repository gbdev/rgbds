def fzero equs "startof(\"test\")"
section "test", rom0
; Signed overflow is undefined behavior, so these must wrap around instead
dl $7fff_ffff + ({fzero} + 1)
dl $8000_0000 - ({fzero} + 1)
dl $7fff_ffff * ({fzero} + 3)

; XXX: We rely on this landing at address $0000, which isn't *guaranteed*...
assert startof("test") == 0
