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
	action pret/pokecrystal 2022-09-29 70a3ec1accb6de1c1c273470af0ddfa2edc1b0a9
	action pret/pokered     2022-09-29 2b52ceb718b55dce038db24d177715ae4281d065
fi
action         AntonioND/ucity  2022-04-20 d8878233da7a6569f09f87b144cb5bf140146a0f
