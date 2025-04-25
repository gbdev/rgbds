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
				git clone "https://github.com/$1/$2.git" --shallow-since="$3" --single-branch
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
	action pret  pokecrystal      2025-04-20 098944485b76d6bd758ead442f3f17ac29aacd39
	action pret  pokered          2025-04-19 d47f74ee1fed608f902dbc0fd51634dd7bafc6e4
	action zladx LADX-Disassembly 2025-04-12 8db9dbbd1dcbbdb4df7c455f56d0ad1fa52b124b
fi
action AntonioND ucity   2025-03-20 04cdda3dda35e62501cf9543b73343935039ee4d
action pinobatch libbet  2025-03-28 c564dc01baa91bd6123ec64d877630dd392e74b5
action LIJI32    SameBoy 2025-04-07 1cf84a5436c49413ae756c9e9c18ce71e96d7990
