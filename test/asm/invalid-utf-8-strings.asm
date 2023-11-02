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
DEF invalid EQUS "aÃ¤bæ¼¢,a£¤bæð¢!"

DEF n = STRLEN("{invalid}")
DEF copy EQUS STRSUB("{invalid}", 1)

println "\"{invalid}\" == \"{copy}\" ({d:n})"

DEF mid1 EQUS STRSUB("{invalid}", 5, 2)
DEF mid2 EQUS STRSUB("{invalid}", 9, 1)
println "\"{mid2}{mid1}\""
