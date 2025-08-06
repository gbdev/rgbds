SECTION "test", ROM0

pusho
	opt p42, -Q.4, Wno-div
	ds 1
	println $8000_0000 / -1
	def n = 3.14
	println "{x:n} = {f:n}"
popo

	ds 1
	println $8000_0000 / -1
	def n = 3.14
	println "{x:n} = {f:n}"

pusho -p99
	ds 1
popo
