MACRO m
  DEF S EQUS \1
  println "{S}"
  println \1
  println "illegal character \
escape \z?"
  println "invalid character \<\n>?"
  println """invalid character \<	>?"""
ENDM
  m "(\n \r \t)"
