#!/bin/false

fetch_action() {
	action codeberg.org ISSOtm gb-starter-kit 74b647d62ff74b40d2b52e585cbebe148463212e
}

test_action() {
	action ISSOtm gb-starter-kit all bin/boilerplate.gb b4f130169ba73284e0d0e71b53e7baa4eca2f7fe
}
