; This used to fail as moving to the next "alignment
; boundary" would wrap across 16 bits.

SECTION "A", HRAM[$FF80]
ds 96 ; fills $FF80..$FFDF, leaving 31 bytes free

SECTION "B", HRAM, ALIGN[7] ; 128-byte alignment
ds 32
