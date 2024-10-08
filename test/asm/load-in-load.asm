SECTION "outer", ROM0
LOAD "inner1", WRAM0 ; starts "inner1"
LOAD "inner2", HRAM ; ends "inner1", starts "inner2"
ENDL ; ends "inner2"
ENDL ; error
