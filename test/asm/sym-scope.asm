SECTION "Scopes", ROM0

; Tests of injecting local labels into another label's scope.
; This construction is useful to define a subroutine's local variables
; in WRAM or HRAM.
Valid.syntax

Parent:
.loc
	dw Parent.loc ; This is what it should expand to
Parent.explicit
	dw .explicit ; This should expand to the above


; Note that `Parentheses` begins with `Parent`,
; which string checks may fail to handle

Parentheses.check

Parentheses:

Parent.check
