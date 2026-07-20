SECTION "high", HRAM[$FFE0]
hLCDInterruptHandler::
.jp: db ; should be $c3 (jp)
.address: dw
