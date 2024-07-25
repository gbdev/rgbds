EXPORT undefined

DEF equ_sym EQU 1
DEF var_sym = 2
EXPORT equ_sym, var_sym

EXPORT DEF constant EQU 42
EXPORT DEF variable = 1337
EXPORT DEF byte RB
EXPORT DEF word RW
EXPORT DEF long RL
EXPORT REDEF constant EQU 69
EXPORT REDEF variable = 1234

; String constants can't be exported or imported.
DEF equs_sym EQUS "hello"
EXPORT equs_sym ; exports undefined symbol `hello` due to EQUS expansion
EXPORT DEF string EQUS "goodbye" ; invalid syntax
