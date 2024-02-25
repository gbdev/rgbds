SECTION "Collision course", OAM[$FE00]

; All the following symbols used to collide with our custom hashmap,
; which at some point caused `PURGE` to malfunction with them.
; We now use C++ `std::map` which reliably handles collisions.
aqfj: ds 1 ; Give them different addresses
cxje: ds 1
dgsd: ds 1
dork: ds 1
lxok: ds 1
psgp: ds 1
sfly: ds 1
syyq: ds 1
ussg: ds 1
xlmm: ds 1
xtzp: ds 1
zxfr: ds 1

	; Completely by accident, but cool
	PURGE dork

	PRINTLN "aqfj: {aqfj}"
	PRINTLN "cxje: {cxje}"
	PRINTLN "dgsd: {dgsd}"
	PRINTLN "dork: {dork}"
	PRINTLN "lxok: {lxok}"
	PRINTLN "psgp: {psgp}"
	PRINTLN "sfly: {sfly}"
	PRINTLN "syyq: {syyq}"
	PRINTLN "ussg: {ussg}"
	PRINTLN "xlmm: {xlmm}"
	PRINTLN "xtzp: {xtzp}"
	PRINTLN "zxfr: {zxfr}"
