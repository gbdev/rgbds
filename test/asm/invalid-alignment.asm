SECTION "a", ROMX[$4000], ALIGN[20] ; invalid

SECTION FRAGMENT "b", ROM0[$0000], ALIGN[20] ; invalid

SECTION UNION "c", WRAM0[$c000], ALIGN[20] ; invalid

SECTION "d", HRAM[$ff80], ALIGN[10] ; unattainable

SECTION FRAGMENT "e", ROMX[$4000], ALIGN[15] ; unattainable

SECTION UNION "f", WRAM0[$c000], ALIGN[15] ; unattainable
