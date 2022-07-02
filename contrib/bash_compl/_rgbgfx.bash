#/usr/bin/env bash

# Same notes as RGBASM

_rgbgfx_completions() {
	COMPREPLY=()

	# Format: "long_opt:state_after"
	# Empty long opt = it doesn't exit
	# See the `state` variable below for info about `state_after`
	declare -A opts=(
		[V]="version:normal"
		[C]="color-curve:normal"
		[m]="mirror-tiles:normal"
		[u]="unique-tiles:normal"
		[v]="verbose:normal"
		[Z]="columns:normal"
		[a]="attr-map:*.attrmap"
		[A]="output-attr-map:normal"
		[b]="base-tiles:unk"
		[d]="depth:unk"
		[L]="slice:unk"
		[N]="nb-tiles:unk"
		[n]="nb-palettes:unk"
		[o]="output:glob *.2bpp"
		[p]="palette:glob *.pal"
		[P]="output-palette:normal"
		[q]="palette-map:glob *.palmap"
		[Q]="output-palette-map:normal"
		[r]="reverse:unk"
		[s]="palette-size:unk"
		[t]="tilemap:glob *.tilemap"
		[T]="output-tilemap:normal"
		[x]="trim-end:unk"
	)
	# Parse command-line up to current word
	local opt_ena=true
	# Possible states:
	# - normal  = Well, normal. Options are parsed normally.
	# - unk     = An argument that can't be completed, and should just be skipped.
	# - warning = A warning flag.
	# - dir     = A directory path
	# - glob-*  = A glob, after the dash is a whitespace-separated list of file globs to use
	local state=normal
	# The length of the option, used as a return value by the function below
	local optlen=0
	# $1: a short option word
	# `state` will be set to the parsing state after the last option character in the word. If
	# "normal" is not returned, `optlen` will be set to the length (dash included) of the "option"
	# part of the argument.
	parse_short_opt() {
		for (( i = 1; i < "${#1}"; i++ )); do
			# If the option is not known, assume it doesn't take an argument
			local opt="${opts["${1:$i:1}"]:-":normal"}"
			state="${opt#*:}"
			# If the option takes an argument, record the length and exit
			if [[ "$state" != 'normal' ]]; then
				let optlen="$i + 1"
				return
			fi
		done
		optlen=0
	}

	for (( i = 1; i < $COMP_CWORD; i++ )); do
		local word="${COMP_WORDS[$i]}"

		# If currently processing an argument, skip this word
		if [[ "$state" != 'normal' ]]; then
			state=normal
			continue
		fi

		if [[ "$word" = '--' ]]; then
			# Options stop being parsed after this
			opt_ena=false
			break
		fi

		# Check if it's a long option
		if [[ "${word:0:2}" = '--' ]]; then
			# If the option is unknown, assume it takes no arguments: keep the state at "normal"
			for long_opt in "${opts[@]}"; do
				if [[ "$word" = "--${long_opt%%:*}" ]]; then
					state="${long_opt#*:}"
					# Check if the next word is just '='; if so, skip it, the argument must follow
					# (See "known bugs" at the top of this script)
					let i++
					if [[ "${COMP_WORDS[$i]}" != '=' ]]; then
						let i--
					fi
					optlen=0
					break
				fi
			done
		# Check if it's a short option
		elif [[ "${word:0:1}" = '-' ]]; then
			parse_short_opt "$word"
			# The last option takes an argument...
			if [[ "$state" != 'normal' ]]; then
				if [[ "$optlen" -ne "${#word}" ]]; then
					# If it's contained within the word, we won't complete it, revert to "normal"
					state=normal
				else
					# Otherwise, complete it, but start at the beginning of *that* word
					optlen=0
				fi
			fi
		fi
	done

	# Parse current word
	# Careful that it might look like an option, so use `--` aggressively!
	local cur_word="${COMP_WORDS[$COMP_CWORD]}"

	# Process options, as short ones may change the state
	if $opt_ena && [[ "$state" = 'normal' && "${cur_word:0:1}" = '-' ]]; then
		# We might want to complete to an option or an arg to that option
		# Parse the option word to check
		# There's no whitespace in the option names, so we can ride a little dirty...

		# Is this a long option?
		if [[ "${cur_word:1:1}" = '-' ]]; then
			# It is, try to complete one
			COMPREPLY+=( $(compgen -W "${opts[*]%%:*}" -P '--' -- "${cur_word#--}") )
			return 0
		else
			# Short options may be grouped, parse them to determine what to complete
			parse_short_opt "$cur_word"

			if [[ "$state" = 'normal' ]]; then
				COMPREPLY+=( $(compgen -W "${!opts[*]}" -P "$cur_word" '') )
				return 0
			elif [[ "$optlen" = "${#cur_word}" && "$state" != "warning" ]]; then
				# This short option group only awaits its argument!
				# Post the option group as-is as a reply so that Readline inserts a space,
				# so that the next completion request switches to the argument
				# An exception is made for warnings, since it's idiomatic to stick them to the
				# `-W`, and it doesn't break anything.
				COMPREPLY+=( "$cur_word" )
				return 0
			fi
		fi
	fi

	case "$state" in
		unk) # Return with no replies: no idea what to complete!
			;;
		normal) # Acts like a glob...
			state="glob-*.png"
			;&
		glob-*)
			while read -r word; do
				COMPREPLY+=("${cur_word:0:$optlen}$word")
			done < <(for glob in ${state#glob-}; do compgen -A file -X \!"$glob" -- "${cur_word:$optlen}"; done)
			# Also complete directories
			;&
		dir)
			while read -r word; do
				COMPREPLY+=("${cur_word:0:$optlen}$word")
			done < <(compgen -A directory -- "${cur_word:$optlen}")
			compopt -o filenames
			;;
	esac
}

complete -F _rgbgfx_completions rgbgfx
