ASSERT !DEF(_NARG)

PURGE _NARG

DEF _NARG EQU 12
REDEF _NARG EQU 34

DEF _NARG = 56
REDEF _NARG = 78

DEF _NARG EQUS "hello"
REDEF _NARG EQUS "world"

SECTION "_NARG", ROM0
_NARG:
ENDSECTION

ASSERT !DEF(.)

PURGE .

DEF . EQU 12
REDEF . EQU 34

DEF . = 56
REDEF . = 78

DEF . EQUS "hello"
REDEF . EQUS "world"

SECTION ".", ROM0
.:
ENDSECTION
