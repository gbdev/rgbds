; characters:
; 1: U+0061 a
; 2: U+00E4 a with diaresis (0xC3 0xA4)
; 3: U+0062 b
; 4: U+6F22 kanji (0xE6 0xBC 0xA2)
; 5: U+002C ,
; 6: U+0061 a
; 7: invalid byte 0xA3
; 8: invalid byte 0xA4
; 9: U+0062 b
; 10: invalid bytes 0xE6 0xF0
; 11: invalid byte 0xA2
; 12: U+0021 !
DEF invalid EQUS "a√§bÊº¢,a£§bÊ¢!"

DEF n = STRLEN("{invalid}")
DEF copy EQUS STRSLICE("{invalid}", 0)

println "\"{#s:invalid}\" == \"{#s:copy}\" ({d:n})"

DEF mid1 EQUS STRSLICE("{invalid}", 4, 6)
DEF mid2 EQUS STRSLICE("{invalid}", 8, 9)
println "\"{#s:mid2}{#s:mid1}\""

; characters:
; 1: U+0041 A
; 2: U+0020 space
; 3: invalid byte 0xFE
; 4: invalid byte 0x81
; 5: invalid byte 0xFF
; 6: U+0020 space
; 7: U+6F22 kanji (0xE6 0xBC 0xA2)
REDEF invalid EQUS "A ˛Åˇ Êº¢"

DEF n = STRLEN("{invalid}")
DEF r = CHARLEN("{invalid}")
println "\"{#s:invalid}\": {d:n} == {d:r}"

REDEF mid1 EQUS STRCHAR("{invalid}", 3)
REDEF mid2 EQUS STRCHAR("{invalid}", 6)
println "\"{#s:mid2}{#s:mid1}\""

; characters:
; 1: U+0061 a
; 2: U+0062 b
; 3: U+0063 c
; 4: incomplete U+6F22 kanji (0xE6 0xBC without 0xA2)
REDEF invalid EQUS "abcÊº"

DEF n = STRLEN("{invalid}")
DEF r = CHARLEN("{invalid}")
println "\"{#s:invalid}\": {d:n} == {d:r}"

DEF final EQUS STRSLICE("{invalid}", 3, 4)
println "\"{#s:invalid}\" ends \"{#s:final}\""
