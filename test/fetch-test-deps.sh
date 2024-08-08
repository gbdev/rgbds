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
	action pret  pokecrystal      2024-08-03 746a06f1de77dd6ffdba76ea33bb0181773326b6
	action pret  pokered          2024-08-03 59202a2ab5f097262db25eb9b12c41d2389c5164
	action zladx LADX-Disassembly 2024-07-31 3283fd6bf899eec9d307ac3a495a01c45f24ceec
fi
action AntonioND ucity   2024-08-03 f3c6377f1fb1ea29644bcd90722abaaa5d478a74
action pinobatch libbet  2024-06-15 ee60f0e4712a938589edd3e5d258e519a475d754
action LIJI32    SameBoy 2024-08-01 47cdefd3d022b31d984ede91cd0f6965ff88d065
