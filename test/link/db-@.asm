SECTION "fixed", ROM0[0]
	db @, @, @

; We need this section to be floating because RGBASM can know the value of PC
; otherwise, leading to different behavior
SECTION "floating", ROM0
	db @, @, @

; We rely on this landing at address $0003, which isn't *guaranteed*...
assert STARTOF("floating") == 3
