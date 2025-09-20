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
	action pret  pokecrystal      2025-09-05 d138ed1bd4db80cf8caa549878600448fedf674e
	action pret  pokered          2025-09-09 59da8c8122ebb8fcc334d4e5421e4fb333eea730
	action zladx LADX-Disassembly 2025-07-26 3ab7b582f67b4302a4f6371f9309f0de167e78ee
fi
action AntonioND ucity          2025-08-07 d1880a2a112d7c26f16c0fc06a15b6c32fdc9137
action pinobatch libbet         2025-08-31 e42c0036b18e6e715987b88b4973389b283974c9
action LIJI32    SameBoy        2025-09-13 6b38c535755b4ff2b069efac404b2844292249b7
action ISSOtm    gb-starter-kit 2025-09-19 746b3d29640037c41fa3220b38feab2452f81dcf
