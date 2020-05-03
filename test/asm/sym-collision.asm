; Hashmap collisions are pretty poorly-tested code path...
; At some point, `PURGE` would malfunction with them

SECTION "Collision course", OAM[$FE00]

; All the following symbols collide!
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

	PRINTT "aqfj: {aqfj}\n"
	PRINTT "cxje: {cxje}\n"
	PRINTT "dgsd: {dgsd}\n"
	PRINTT "dork: {dork}\n"
	PRINTT "lxok: {lxok}\n"
	PRINTT "psgp: {psgp}\n"
	PRINTT "sfly: {sfly}\n"
	PRINTT "syyq: {syyq}\n"
	PRINTT "ussg: {ussg}\n"
	PRINTT "xlmm: {xlmm}\n"
	PRINTT "xtzp: {xtzp}\n"
	PRINTT "zxfr: {zxfr}\n"
