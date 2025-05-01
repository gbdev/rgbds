SECTION "rom0", ROM0
Label0:: ds 1
.local::
:

SECTION "romx", ROMX
Label1:: ds 2
.local::
:

SECTION "vram", VRAM
vLabel:: ds 3
.local::
:

SECTION "sram", SRAM
sLabel:: ds 4
.local::
:

SECTION "wram0", WRAM0
wLabel0:: ds 5
.local::
:

SECTION "wramx", WRAMX
wLabel1:: ds 6
.local::
:

SECTION "hram", HRAM
hLabel:: ds 7
.local::
:

SECTION "\n\r\t\"\\", ROM0[1]
