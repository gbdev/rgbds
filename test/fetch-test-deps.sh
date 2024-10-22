#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
	echo "Downloads source code of Game Boy programs used as RGBDS test cases."
	echo "Options:"
	echo "    -h, --help      show this help message"
	echo "    --only-free     download only freely licensed codebases"
	echo "    --get-deps      install programs' own dependencies instead of themselves"
	echo "    --get-hash      print programs' commit hashes instead of downloading them"
	echo "    --get-paths     print programs' GitHub paths instead of downloading them"
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
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
					macos)
						python3 -m pip install --break-system-packages pillow
						;;
					ubuntu)
						python3 -m pip install pillow
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

if "$nonfree"; then
	action pret  pokecrystal      2024-10-16 961fad9e150df324afc9bccd1ce15c3a65d3c124
	action pret  pokered          2024-10-15 a891fc1168a7f998c570e1ea5f15014556df2d95
	action zladx LADX-Disassembly 2024-09-16 008d01541f8cab3f4590cbc94a690af2b9a7979f
fi
action AntonioND ucity   2024-08-03 f3c6377f1fb1ea29644bcd90722abaaa5d478a74
action pinobatch libbet  2024-10-20 71d04e850534cbe77d1c143153f664dac1960bc9
action LIJI32    SameBoy 2024-10-12 52d5169cc82356288f337d30aa01fb3fa1b37155
