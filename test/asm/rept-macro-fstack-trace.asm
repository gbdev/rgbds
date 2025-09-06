MACRO outer
  DEF it = 1
  DEF n = 0
  REPT 4
    IF n % 2 == 0
      IF DEF(inner)
        PURGE inner
      ENDC
      DEF s EQUS "\nMACRO inner\nREPT 2\nREPT 2\nWARN \"round \{d:it\}\"\nDEF it += 1\nENDR\nENDR\nENDM"
      {s}
      PURGE s
    ENDC
    inner
    DEF n += 1
  ENDR
ENDM
REPT 1
  outer
ENDR

MACRO foo
  REPT 1
    REPT 1
      WARN "round {d:it}"
      DEF it += 1
    ENDR
  ENDR
ENDM
REPT 1
  REPT 1
    MACRO bar
      REPT 2
        REPT 2
          foo
        ENDR
      ENDR
    ENDM
    bar
  ENDR
ENDR

