SECTION "root", ROM0[0]
    dw WRAMLabel

; This section should be kept thanks to the reference from the WRAM section
SECTION "A", ROM0
Label:
    db $01, $02
.end:

SECTION "wram", WRAM0
WRAMLabel:
    ds Label.end - Label

SECTION "UnRef", ROM0
UnRef:
    db UnRef
