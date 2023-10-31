DEF X = 42
PRINTLN "{X}"
PRINTLN "{x:X}"
PRINTLN "{X:X}"
PRINTLN "{d:X}"
PRINTLN "{b:X}"

DEF Y EQU 1337
PRINTLN "{b:Y}"

rsreset
DEF R RB 0
PRINTLN "{d:R}"

DEF S EQUS "You can't format me!"
PRINTLN "{X:S}"

SECTION "Test", ROM0
Label:
PRINTLN "{x:Label}"
PRINTLN "{x:@}"
