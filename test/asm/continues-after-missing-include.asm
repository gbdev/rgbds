PUSHC
PUSHO
PUSHS
SECTION "test", WRAM0
UNION
INCLUDE "nonexistent1.inc"
WARN "still going!"
INCLUDE "nonexistent2.inc"
WARN "and going!"
ENDU
POPS
POPO
POPC
