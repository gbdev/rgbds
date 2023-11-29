; Test text after \ line continuation

MACRO \ spam
  WARN "spam"
ENDM
  spam ; The macro was defined despite the error

; Test that \ after a macro invocation at the end of the file doesn't
; cause a segfault.

MACRO bar
ENDM

foo: bar baz\
