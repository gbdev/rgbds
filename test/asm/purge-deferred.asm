DEF prefix EQUS "cool"
DEF {prefix}banana EQU 1

ASSERT DEF(prefix)
ASSERT DEF(coolbanana)

PURGE prefix, {prefix}banana

ASSERT !DEF(prefix)
ASSERT !DEF(coolbanana)
