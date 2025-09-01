section "rom", rom0
ld bc, -wLabel
ld de, -(wLabel * 2)

section "ram", wram0
wLabel::
