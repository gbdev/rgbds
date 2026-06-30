#!/bin/false

fetch_action() {
	action github.com pret pokered 0555b42dc0ceffaae613e97cc0cf2e8c0b45013c
}

test_action() {
	action pret pokered compare pokered.gbc ea9bcae617fdf159b045185467ae58b2e4a48b9a
}
