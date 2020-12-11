
SECTION "Label testing", WRAMX

	Lab:
	.loc
	Lab.loc2

;	X = 0
;	; Should not believe X is a label!
;	IF X == 1
;		FAIL "Wrong!"
;	ENDC
;	X \
;	  = 1
;	IF X != 1
;		FAIL "Wrong!"
;	ENDC
;
;	Y equ 42
;	PRINTT "Y={Y}\n"
;	PURGE Y
;
;	Y equs "mac"
;	mac: MACRO
;		PRINTT "\1\n"
;	ENDM
;	; Should invoke macro Y, and not match "equ" due to the "a" behind it
;	Y equates 69
;	Y equsquisite (isn't that English?)
;	Y eq
