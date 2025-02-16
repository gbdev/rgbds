FOR V, 1, 100
      PRINTLN "- {d:V}"
      IF V == 5
          PRINTLN "stop"
          BREAK
      ENDC
      PRINTLN "cont"
ENDR
WARN "done {d:V}"
rept 2
  break
  ; skips nested code
  rept 3
    println "\tinner"
  endr
  if 1
    println "\tconditional"
  endc
  println "outer"
endr
rept 1
  break
  ; skips invalid code
  !@#$%
macro elif
  invalid
endr
warn "OK"
break ; not in a rept/for
rept 1
  if 1
    break
  no endc
endr
println "done"
