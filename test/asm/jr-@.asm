SECTION "fixed", ROM0[0]
	jr @

; We need this section to be floating because RGBASM can know the value of PC
; otherwise, leading to different behavior
SECTION "floating", ROM0
	jr @

; We rely on this landing at address $0002, which isn't *guaranteed*...
assert STARTOF("floating") == 2
