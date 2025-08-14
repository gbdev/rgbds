MACRO outer
    DEF s EQUS "MACRO inner\nREPT 1\nWARN \"hello\"\nENDR\nENDM\ninner\n"
    {s}
    PURGE s
    inner
ENDM
outer
inner
