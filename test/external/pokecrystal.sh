#!/bin/false

fetch_action() {
	action github.com pret pokecrystal 2bbb15675de0d2bbebc8cc9978f5c7fb15bc73b9
}

test_action() {
	action pret pokecrystal compare pokecrystal.gbc f4cd194bdee0d04ca4eac29e09b8e4e9d818c133
}
