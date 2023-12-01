MACRO m
  println "  m: \@"
  for q, \#
    if q % 2 == 0
      println "    q = {d:q}: \@"
    else
      println "    q = {d:q}"
    endc
  endr
  println "  (m: still \@)"
ENDM

for p, 10
  if p % 3 == 0
    println "p = {d:p}: \@"
    m 3
    m 3, 6
    println " (p: still \@)"
  else
    println "p = {d:p}"
  endc
endr

rept 3
  if def(m)
    purge m
  endc
  MACRO m
    println "go \1 \@"
  ENDM
  m \@
endr
  m outside
