opt Wno-unmapped-char
def s equs "Hello world!"

static_assert strin(#s, "l") == strfind(#s, "l") + 1
static_assert strrin(#s, "l") == strrfind(#s, "l") + 1

static_assert strsub(#s, 7) === strslice(#s, 6)
static_assert strsub(#s, 7, 5) === strslice(#s, 6, 11)
static_assert strsub(#s, strlen(#s), 1) === strslice(#s, strlen(#s) - 1, strlen(#s))
static_assert strsub(#s, 7, 999) === strslice(#s, 6, 999)

static_assert charsub(#s, 12) === strchar(#s, 11)
static_assert charsub(#s, -1) === strchar(#s, -1)
static_assert charsub(#s, -999) === strchar(#s, -999)
static_assert charsub(#s, 999) === strchar(#s, 999)

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
