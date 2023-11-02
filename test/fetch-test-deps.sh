#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

usage() {
	echo "Downloads source code of Game Boy programs used as RGBDS test cases."
	echo "Options:"
	echo "    -h, --help      show this help message"
	echo "    --only-free     download only freely licensed codebases"
	echo "    --get-hash      print programs' commit hashes instead of downloading them"
	echo "    --get-paths     print programs' GitHub paths instead of downloading them"
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
actionname=
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-free)
			nonfree=false
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
	action pret/pokecrystal 2023-10-22 38667169809b81eb39990b4341f9919332d27248
	action pret/pokered     2023-10-10 b302e93674f376f2881cbd931a698345ad27bec3
fi
action         AntonioND/ucity  2023-11-01 8a6342caf003652f3038a34834209e85026979c0
