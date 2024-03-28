#!/usr/bin/env bash
set -e

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
					ubuntu|macos)
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
	action pret  pokecrystal      2024-03-03 c1da20e2f12f95c935500151d15f455e7e7eb213
	action pret  pokered          2024-01-02 fabe2b3fb3fb5a849c5220298acabbdc9ad30f3b
	action zladx LADX-Disassembly 2024-02-25 583c78d2f2a5258b87bf133f75b7129228255650
fi
action AntonioND ucity   2023-11-02 c781ae20c0b319262b19b51e5067a2c93cf3b362
action pinobatch libbet  2024-03-09 c98c7574d94f3e1a977bd7e98899f30a1ecbbb17
action LIJI32    SameBoy 2024-03-08 e7792c16b24c08f55a370973f0beaecb7bd0ab92
