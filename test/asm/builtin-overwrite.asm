macro tickle
; There once was a bug where overwriting worked only on the second try, so
; try everything twice for good measure

PURGE \1
PURGE \1
PRINTLN \1

DEF \1 EQU 0
DEF \1 EQU 0
PRINTLN \1

DEF \1 = 0
DEF \1 = 0
PRINTLN \1

DEF \1 EQUS "hello"
DEF \1 EQUS "hello"
PRINTLN \1

REDEF \1 = 0
REDEF \1 = 0
PRINTLN \1

REDEF \1 EQU 0
REDEF \1 EQU 0
PRINTLN \1

REDEF \1 EQUS "hello"
REDEF \1 EQUS "hello"
PRINTLN \1
endm

    ; Representative numeric and string builtins
    ; (SOURCE_DATE_EPOCH in test.sh makes this reproducible)
    tickle __UTC_YEAR__, 1
    tickle __ISO_8601_UTC__, 0
