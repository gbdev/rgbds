; Macro invocations not followed by a newline may scan an EOF;
; If this is the case, it shouldn't cause the parse to end before the macro is expanded
mac: macro
	PRINTLN "Hi beautiful"
endm
	mac