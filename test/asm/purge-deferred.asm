DEF prefix EQUS "cool"
DEF {prefix}banana EQU 1

STATIC_ASSERT DEF(prefix)
STATIC_ASSERT DEF(coolbanana)

; purging `prefix` should not prevent expanding it to purge `coolbanana`
PURGE prefix, {prefix}banana

STATIC_ASSERT !DEF(prefix)
STATIC_ASSERT !DEF(coolbanana)
