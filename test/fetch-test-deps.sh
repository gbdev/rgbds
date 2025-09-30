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
		action() { # _ _ repo _ _
			# libbet depends on PIL to build
			if [ "$3" = "libbet" ]; then
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
		action() { # _ _ repo _ commit
			printf "%s@%s-" "$3" "$5"
		}
		;;

	--get-paths)
		action() { # _ _ repo _ _
			printf "test/%s," "$3"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() { # domain owner repo shallow-since commit
			if [ ! -d "$3" ]; then
				git clone "https://$1/$2/$3.git" --recursive --shallow-since="$4" --single-branch
			fi
			pushd "$3"
			git checkout -f "$5"
			if [ -f "../patches/$3.patch" ]; then
				git apply --ignore-whitespace "../patches/$3.patch"
			fi
			popd
		}
esac

if ! "$external"; then
	exit
fi

if "$nonfree"; then
	action github.com pret  pokecrystal      2025-09-05 d138ed1bd4db80cf8caa549878600448fedf674e
	action github.com pret  pokered          2025-09-25 628797baffe7ea7dd4b224116d9704c7ae1b9c29
	action github.com zladx LADX-Disassembly 2025-09-20 e09ee3259acbdecb89a0eba6cbc438281c174e85
fi
action github.com   AntonioND ucity          2025-08-07 d1880a2a112d7c26f16c0fc06a15b6c32fdc9137
action github.com   pinobatch libbet         2025-08-31 e42c0036b18e6e715987b88b4973389b283974c9
action github.com   LIJI32    SameBoy        2025-09-27 91006369a6510c5db029a440691dd4becaa6208b
action codeberg.org ISSOtm    gb-starter-kit 2025-09-23 6aeb2508ab75c15724b177a1437b939357bc5d6f
