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
			echo "$(basename $0): unknown option "$1""
			exit 1
			;;
	esac
	shift
done

case "$actionname" in
	--get-deps)
		action() { # # owner/repo
			# libbet depends on PIL to build
			if [ "$1" = "pinobatch/libbet" ]; then
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
		action() { # owner/repo shallow-since commit
			printf "%s@%s-" "${1##*/}" "$3"
		}
		;;

	--get-paths)
		action() { # owner/repo shallow-since commit
			printf "test/%s," "${1##*/}"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() { # owner/repo shallow-since commit
			if [ ! -d ${1##*/} ]; then
				git clone https://github.com/$1.git --shallow-since=$2 --single-branch
			fi
			pushd ${1##*/}
			git checkout -f $3
			if [ -f ../patches/${1##*/}.patch ]; then
				git apply --ignore-whitespace ../patches/${1##*/}.patch
			fi
			popd
		}
esac

if "$nonfree"; then
	action pret/pokecrystal       2023-11-22 9a917e35760210a1f34057ecada2148f1fefc390
	action pret/pokered           2023-12-05 f6017ddbfd7e14ea39b81ce3393de9117e7310d9
	action zladx/LADX-Disassembly 2023-12-10 e08773051fa75f0c02a85434196fd598642d2c2c
fi
action AntonioND/ucity  2023-11-02 c781ae20c0b319262b19b51e5067a2c93cf3b362
action pinobatch/libbet 2023-11-30 9f56cf94883c58517c2cd41a752cfee5a5800e8d
action LIJI32/SameBoy   2023-12-05 8642715a6d75520753196de96b73ada4451739a4
