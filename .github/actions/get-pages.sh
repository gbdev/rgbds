#!/bin/bash

usage() {
	cat <<EOF
Usage: $0 [-h] [-r] <rgbds-www> <version>
Copy renders from RGBDS repository to rgbds-www documentation
Execute from the \`man/\` folder in the RGBDS repo, checked out at the desired tag
<rgbds-www> : Path to the rgbds-www repository
<version>   : Version to be copied, such as 'v0.4.1' or 'master'

    -h  Display this help message
    -r  Update "latest stable" redirection pages and add a new entry to the index
        (use for releases, not master)
EOF
}

is_release=0
bad_usage=0
while getopts ":hr" opt; do
	case $opt in
		r)
			is_release=1
			;;
		h)
			usage
			exit 0
			;;
		\?)
			echo "Unknown option '$OPTARG'"
			bad_usage=1
			;;
	esac
done
if [ $bad_usage -ne 0 ]; then
	usage
	exit 1
fi
shift $((OPTIND - 1))


declare -a PAGES
PAGES=(
	rgbasm.1
	rgbasm.5
	rgblink.1
	rgblink.5
	rgbfix.1
	rgbgfx.1
	rgbds.5
	rgbds.7
	gbz80.7
)
WWWPATH="/docs"
OUTDIR="$1/_documentation/$2"
mkdir -p "$OUTDIR"

# `mandoc` uses a different format for referring to man pages present in the **current** directory.
# We want that format for RGBDS man pages, and the other one for the rest;
# this script must thus be run from the directory all man pages are in.

for page in "${PAGES[@]}"; do
manpage="${page%.?}(${page#*.})" # "rgbasm(5)"
descr="$(awk -v 'FS=.Nd ' '/.Nd/ { print $2; }' "$page")" # "language documentation"

	cat >"$OUTDIR/$page.html" <<EOF
---
layout: doc
title: $manpage [$2]
description: RGBDS $2 — $descr
---
EOF
	options=fragment,man='%N.%S;https://linux.die.net/man/%S/%N'
	if [[ $page = rgbasm.5 ]]; then
		options+=,toc
	fi
	mandoc -W warning -Thtml -I os=Linux -O$options "$page" | ../.github/actions/doc_postproc.awk >> "$OUTDIR/$page.html"
	groff -Tpdf -mdoc -wall "$page" >"$OUTDIR/$page.pdf"
	if [[ $is_release -ne 0 ]]; then
		cat - >"$1/_documentation/$page.html" <<EOF
---
redirect_to: $WWWPATH/$2/$page
permalink: $WWWPATH/$page/
title: $manpage [latest stable]
description: RGBDS latest stable — $descr
---
EOF
	fi
done

cat - >"$OUTDIR/index.html" <<EOF
---
layout: doc_index
permalink: /docs/$2/
title: RGBDS online manual [$2]
description: RGBDS $2 - Online manual
---
EOF


# If making a release, add a new entry right after `master`
if [[ $is_release -ne 0 ]]; then
	awk '{ print }
/"name": "master"/ { print "\t\t{\"name\": \"'"$2"'\",  \"text\": \"'"$2"'\" }," }
' "$1/_data/doc.json" >"$1/_data/doc.json.tmp"
	mv "$1/_data/doc.json"{.tmp,}
fi
