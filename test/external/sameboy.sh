#!/bin/false

fetch_action() {
	action github.com LIJI32 SameBoy 2f4a6f231ec40ecfc0ab7df0a09eb932e7ccddec
}

test_action() {
	action LIJI32 SameBoy bootroms build/bin/BootROMs/cgb_boot.bin 113903775a9d34b798c2f8076672da6626815a91
}
