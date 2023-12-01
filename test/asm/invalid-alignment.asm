; valid

SECTION "valid", ROM0
align 16 ; this is achievable at $0000

; invalid

SECTION "invalid", ROMX[$4000]
align 16

SECTION "a", ROMX[$4000], ALIGN[20]

SECTION FRAGMENT "b", ROM0[$0000], ALIGN[20]

SECTION UNION "c", WRAM0[$c000], ALIGN[20]

; unattainable

SECTION "d", HRAM[$ff80], ALIGN[10]

SECTION FRAGMENT "e", ROMX[$4000], ALIGN[15]

SECTION UNION "f", WRAM0[$c000], ALIGN[15]
