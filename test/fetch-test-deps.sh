#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
	cat <<"EOF"
Downloads source code of Game Boy programs used as RGBDS test cases.
Options:
    -h, --help      show this help message
    --only-free     download only freely licensed codebases
    --get-hash      print programs' commit hashes instead of downloading them
    --get-paths     print programs' GitHub paths instead of downloading them
EOF
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
	--get-hash)
		action() { # _ _ repo commit
			printf "%s@%s-" "$3" "$4"
		}
		;;

	--get-paths)
		action() { # _ _ repo _
			printf "test/%s," "$3"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() { # domain owner repo commit
			if [ ! -d "$3" ]; then
				git clone "https://$1/$2/$3.git" --revision="$4" --depth=1 --recursive --shallow-submodules --config advice.detachedHead=false
			fi
			pushd "$3"
			git checkout --force --detach "$4" --
			if [ -f "../patches/$3.patch" ]; then
				git apply --ignore-whitespace "../patches/$3.patch"
			fi
			popd
		}
esac

# Sourcing each "external/*.sh" file defines a `fetch_action` function, which calls the
# above `action` function with the appropriate arguments for its external repository.
if "$nonfree"; then
	. external/pokecrystal.sh && fetch_action
	. external/pokered.sh     && fetch_action
	. external/ladx.sh        && fetch_action
fi
. external/ucity.sh          && fetch_action
. external/libbet.sh         && fetch_action
. external/sameboy.sh        && fetch_action
. external/gb-starter-kit.sh && fetch_action
