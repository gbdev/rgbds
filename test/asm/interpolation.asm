SECTION "Test", ROM0[123]

def NAME equs "ITEM"
def FMT equs "d"
def ZERO_NUM equ 0
def ZERO_STR equs "0"
; Defines INDEX as 100
def INDEX = 1{ZERO_STR}{{FMT}:ZERO_NUM}
; Defines ITEM_100 as "\"hundredth\""
def {NAME}_{d:INDEX} equs "\"hundredth\""
; Prints "ITEM_100 is hundredth"
PRINTLN STRCAT("{NAME}_{d:INDEX}", " is ", {NAME}_{d:INDEX})
; Purges ITEM_100
PURGE {NAME}_{d:INDEX}
ASSERT !DEF({NAME}_{d:INDEX})

; undefined
PRINTLN "undef {undef}"

; referenced but undefined
ld hl, label
PRINTLN "label {label}"
label::
PRINTLN "label {label}"

; not string or number
MACRO foo
ENDM
PRINTLN "foo {foo}"

; hashless keyword
PRINTLN "xor {xor}"
