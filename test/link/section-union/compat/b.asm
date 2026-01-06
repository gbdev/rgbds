SECTION "R1", WRAM0
        ds $77

SECTION "R2", WRAM0
        ds $ef

SECTION UNION "U", WRAM0
        ds $52e

SECTION UNION "U", WRAM0
wStart::
        ds $89
        assert @ & $FF == 0, "wContent must be 8-bit aligned"
        align 8
wContent::
        ds $111
wEnd::
