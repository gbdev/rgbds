; With `-w`, there is not enough WRAM (0 & X) to accomodate $4000 bytes;
; without it, there is not enough WRAM0 to accomodate $4001 bytes.
SECTION "w0a", WRAM0
DS $1000

SECTION "w0b", WRAM0
DS 1

SECTION "wx0", WRAMX
DS $1000

SECTION "wx1", WRAMX
DS 1
