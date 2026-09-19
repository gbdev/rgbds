SECTION "rom0", ROM0
Label0:: ds 1
.local::
:

SECTION "romx", ROMX
xLabel:: ds 2
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

SECTION "empty rom0", ROM0[2]

SECTION "rom0 again", ROM0
Label1:: ds 10

SECTION "empty rom0 again", ROM0[8]

SECTION "rom0 yet again", ROM0
Label2:: ds 10

SECTION "rom0 lonely", ROM0[$100]
FarAway::
