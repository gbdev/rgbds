SECTION "fixed", ROM0[$1234]

SECTION "aligned", ROMX, ALIGN[8, 42]

SECTION "less aligned", ROMX, ALIGN[4, 1]

SECTION "more aligned", ROMX, ALIGN[8, 1]

SECTION "address compatible", ROMX[$4463]

SECTION "align compatible", ROMX, ALIGN[8, 99]
