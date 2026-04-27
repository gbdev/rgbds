section "test", ROM0
Global: ; none of the `end*.local_*` identifiers below should get treated as keywords

if 0
	println "no"
endc.local_no
if 1
	println "yes"
endc.local_yes

rept 0
	println "nope"
endr.local_nope
rept 1
	println "yep"
endr.local_yep

macro mac
	println "mac"
endm.local_mac
mac
