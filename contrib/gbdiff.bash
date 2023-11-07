#!/usr/bin/env bash

# SPDX-License-Identifier: MIT

STATE=0
diff <(xxd "$1") <(xxd "$2") | while read -r LINE; do
	if [[ $STATE -eq 0 ]]; then
		# Discard first line (line info)
		STATE=1
	elif [[ "$LINE" = '---' ]]; then
		# Separator between files switches states
		echo "$LINE"
		STATE=3
	elif grep -Eq '^[0-9]+(,[0-9]+)?[cd][0-9]+(,[0-9]+)?' <<< "$LINE"; then
		# Line info resets the whole thing
		STATE=1
	elif [[ $STATE -eq 1  ||  $STATE -eq 3 ]]; then
		# Compute the GB address from the ROM offset
		OFS=$(cut -d ' ' -f 2 <<< "$LINE" | tr -d ':')
		BANK=$((0x$OFS / 0x4000))
		ADDR=$((0x$OFS % 0x4000 + (BANK != 0) * 0x4000))
		# Try finding the preceding symbol closest to the diff
		if [[ $STATE -eq 1 ]]; then
			STATE=2
			SYMFILE=${1%.*}.sym
		else
			STATE=4
			SYMFILE=${2%.*}.sym
		fi
		EXTRA=$(if [[ -f "$SYMFILE" ]]; then
			# Read the sym file for such a symbol
			# Ignore comment lines, only pick matching bank
			# (The bank regex ignores comments already, make `cut` and `tr` process less lines)
			grep -Ei "$(printf "^%02x:" $BANK)" "$SYMFILE" |
			 cut -d ';' -f 1 |
			 tr -d "\r" |
			 while read -r SYMADDR SYM; do
				SYMADDR=$((0x${SYMADDR#*:}))
				if [[ $SYMADDR -le $ADDR ]]; then
					printf " (%s+0x%x)\n" "$SYM" $((ADDR - SYMADDR))
				fi
			# TODO: assumes sorted sym files
			done | tail -n 1
		fi)
		printf "%02x:%04x %s\n" $BANK $ADDR "$EXTRA"
	fi
	if [[ $STATE -eq 2  ||  $STATE -eq 4 ]]; then
		OFS=$(cut -d ' ' -f 2 <<< "$LINE" | tr -d ':')
		BANK=$((0x$OFS / 0x4000))
		ADDR=$((0x$OFS % 0x4000 + (BANK != 0) * 0x4000))
		printf "%s %02x:%04x: %s\n" "${LINE:0:1}" $BANK $ADDR "${LINE#*: }"
	fi
done
