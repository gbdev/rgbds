section "test", rom0
for v1, 4
 if v1 == 3
  for v2, 3
   if v2 == 2
    for v3, 2
     if v3 == 1
      rept 1
       macro m
        assert \1
       endm
      endr
      rept 1
       rept 2
        m @
       endr
      endr
     endc
    endr
   endc
  endr
 endc
endr
