SECTION "RAM", WRAM0

wFoo:: db
wBar:: ds 3
	println "ok"
wQux:: dw [[
	ds 4
	println "inline"
]]
