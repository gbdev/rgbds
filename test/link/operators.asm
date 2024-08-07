SECTION "test", ROM0
ds 4

dw @ + 1
dw @ - 1
dw @ * 2
dw @ / 2
dw @ % 2
dw -@
dw @ ** 2

dw @ | %10101010
dw @ & %10101010
dw @ ^ %10101010
dw ~@

db @ && @
db @ || @
db !@
db @ == 7
db @ != 7
db @ > 7
db @ < 7
db @ >= 7
db @ <= 7

dw @ << 1
dw @ >> 1
dw @ >>> 1

db BITWIDTH(@)
db TZCOUNT(@)
