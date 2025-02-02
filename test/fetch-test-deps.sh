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
	action pret  pokecrystal      2024-12-28 86a87a355e6805663cfecf150a3af71c66441a3e
	action pret  pokered          2025-01-29 43f21cc4948682b51b4c72bcef38631f95b47bfe
	action zladx LADX-Disassembly 2025-01-06 e5ab607f39b0a4bd35f0bae6c6354fde9eecca3c
fi
action AntonioND ucity   2024-12-26 2056166b0a84271d6c2a5a1f7d037dd416085796
action pinobatch libbet  2024-12-26 de2081260d6111a3e60b16120bbb905ef00c6a34
action LIJI32    SameBoy 2025-01-23 1798edf4c1f8cbf018d14436f14c2a18ff256ae4
