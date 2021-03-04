; Fully constrained sections need to be kept
SECTION "root", ROM0[0]


; This section is fully constrained and should be kept
SECTION "01", ROM0[1]
    db $01

; This section is not fully constrained and should not be kept, bank is unknown.
SECTION "02", ROMX[$4002]
    db $02

; This section is fully constrained and should be kept
SECTION "03", ROMX[$4003], BANK[1]
    db $03
