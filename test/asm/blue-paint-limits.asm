macro outer_interp
  def s equs "macro inner_interp\1\nprintln \"interp: \1 and \\1\"\nendm"
  {s}
  purge s
endm
; this prints "1 and red" and "2 and blue"...
outer_interp 1
inner_interp1 red
outer_interp 2
inner_interp2 blue

macro do
  \#
endm
macro outer_mac
  do macro inner_mac\1\nprintln "mac: \1 and \\1"\nendm
endm
; ...but this prints "1 and \1" and "2 and \1"
outer_mac 1
inner_mac1 red
outer_mac 2
inner_mac2 blue

macro outer_concat
  do macro inner_concat\1\nprintln "concat: \1 and "++\\1\nendm
endm
; this does print "1 and red" and "2 and blue" though
outer_concat 1
inner_concat1 "red"
outer_concat 2
inner_concat2 "blue"

macro simple
  println "(\1)"
endm
; this prints """("\\1")""", not """("\1")"""
simple "\\1"

macro nth
  println \1
endm
; this does not print "lol", it's an error
nth \\2, lol
