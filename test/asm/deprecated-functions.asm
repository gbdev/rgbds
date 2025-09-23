opt Wno-unmapped-char
def s equs "Hello world!"

assert strin(#s, "l") == strfind(#s, "l") + 1
assert strrin(#s, "l") == strrfind(#s, "l") + 1

assert strsub(#s, 7) === strslice(#s, 6)
assert strsub(#s, 7, 5) === strslice(#s, 6, 11)
assert strsub(#s, strlen(#s), 1) === strslice(#s, strlen(#s) - 1, strlen(#s))
assert strsub(#s, 7, 999) === strslice(#s, 6, 999)

assert charsub(#s, 12) === strchar(#s, 11)
assert charsub(#s, -1) === strchar(#s, -1)
assert charsub(#s, -999) === strchar(#s, -999)
assert charsub(#s, 999) === strchar(#s, 999)

; characters:
; 1: U+0061 a
; 2: U+00E4 a with diaresis (0xC3 0xA4)
; 3: U+0062 b
; 4: invalid byte 0xA3
; 5: U+0063 c
; 6: incomplete U+6F22 kanji (0xE6 0xBC without 0xA2)
def invalid equs "aÃ¤b£cæ¼"

def copy equs strsub(#invalid, 1)
def past equs strsub(#invalid, 9, 1)
def incomplete equs strsub(#invalid, 12, 1)
