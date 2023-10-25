DEF prefix EQUS "cool"
DEF {prefix}banana EQU 1

ASSERT DEF(prefix)
ASSERT DEF(coolbanana)

; purging `prefix` should not prevent expanding it to purge `coolbanana`
PURGE prefix, {prefix}banana

ASSERT !DEF(prefix)
ASSERT !DEF(coolbanana)
