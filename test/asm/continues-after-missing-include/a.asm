PUSHC
PUSHO
PUSHS
SECTION "test", WRAM0
UNION
INCLUDE "nonexistent1.inc"
WARN "still going!"
NEXTU
INCLUDE "nonexistent2.inc"
WARN "and going!"
ENDU
POPS
POPO
POPC
