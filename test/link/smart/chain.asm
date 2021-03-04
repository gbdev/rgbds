; If multiple sections are chained together by references
; they all should be kept if they can be traced back to a root
SECTION "root", ROM0[0]
    db LabelA

SECTION "A", ROM0
LabelA:
    db LabelB
    ds 3, $EE

SECTION "B", ROM0
LabelB:
    db LabelC
    ds 2, $EE

SECTION "C", ROM0
LabelC:
    db $ff
    ds 1, $EE

SECTION "unref", ROM0
UnRef:
    db UnRef
