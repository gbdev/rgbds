#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
	echo "Downloads source code of Game Boy programs used as RGBDS test cases."
	echo "Options:"
	echo "    -h, --help          show this help message"
	echo "    --only-free         download only freely licensed codebases"
	echo "    --only-internal     do not download any codebases"
	echo "    --get-deps          install programs' own dependencies instead of themselves"
	echo "    --get-hash          print programs' commit hashes instead of downloading them"
	echo "    --get-paths         print programs' GitHub paths instead of downloading them"
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
external=true
actionname=
osname=
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-free)
			nonfree=false
			;;
		--only-internal)
			external=false
			;;
		--get-deps)
			actionname="$1"
			shift
			osname="$1"
			;;
		--get-hash|--get-paths)
			actionname="$1"
			;;
		--)
			break
			;;
		*)
			echo "$(basename "$0"): unknown option '$1'"
			exit 1
			;;
	esac
	shift
done

case "$actionname" in
	--get-deps)
		action() { # _ repo _ _
			# libbet depends on PIL to build
			if [ "$2" = "libbet" ]; then
				case "${osname%-*}" in
					ubuntu | debian)
						sudo apt-get install python3-pil
						;;
					macos)
						python3 -m pip install --break-system-packages pillow
						;;
					windows)
						py -3 -m pip install pillow
						;;
					*)
						echo "WARNING: Cannot install Pillow for OS '$osname'"
						;;
				esac
			fi
		}
		;;

	--get-hash)
		action() { # _ repo _ commit
			printf "%s@%s-" "$2" "$4"
		}
		;;

	--get-paths)
		action() { # _ repo
			printf "test/%s," "$2"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() { # owner repo shallow-since commit
			if [ ! -d "$2" ]; then
				git clone "https://github.com/$1/$2.git" --recursive --shallow-since="$3" --single-branch
			fi
			pushd "$2"
			git checkout -f "$4"
			if [ -f "../patches/$2.patch" ]; then
				git apply --ignore-whitespace "../patches/$2.patch"
			fi
			popd
		}
esac

if ! "$external"; then
	exit
fi

if "$nonfree"; then
	action pret  pokecrystal      2025-07-21 306d1497c15d00631516781fba1606d9e78eb2e1
	action pret  pokered          2025-07-21 fd86bcaa06fd81911e88eb6d0253715ad79866a8
	action zladx LADX-Disassembly 2025-07-26 3ab7b582f67b4302a4f6371f9309f0de167e78ee
fi
action AntonioND ucity          2025-08-07 d1880a2a112d7c26f16c0fc06a15b6c32fdc9137
action pinobatch libbet         2025-05-20 bb6cfc026644aa1034eee6d9c49bb4705601c9f6
action LIJI32    SameBoy        2025-07-26 410719854885b60df8566a444c81bc767965f477
action ISSOtm    gb-starter-kit 2025-06-13 3cce3215b0bcb559a5cf4ca5916e2d817d53ff29
