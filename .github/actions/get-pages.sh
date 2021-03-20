#!/bin/bash

usage() {
	cat <<EOF
Usage: $0 [-h] [-r] <rgbds-www> <version>
Copy renders from RGBDS repository to rgbds-www documentation
Execute from the root folder of the RGBDS repo, checked out at the desired tag
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
shift $(($OPTIND - 1))


declare -A PAGES
PAGES=(
	[rgbasm.1.html]=src/asm/rgbasm.1
	[rgbasm.5.html]=src/asm/rgbasm.5
	[rgblink.1.html]=src/link/rgblink.1
	[rgblink.5.html]=src/link/rgblink.5
	[rgbfix.1.html]=src/fix/rgbfix.1
	[rgbgfx.1.html]=src/gfx/rgbgfx.1
	[rgbds.5.html]=src/rgbds.5
	[rgbds.7.html]=src/rgbds.7
	[gbz80.7.html]=src/gbz80.7
)
WWWPATH="/docs"
mkdir -p "$1/_documentation/$2"

# `mandoc` uses a different format for referring to man pages present in the **current** directory.
# We want that format for RGBDS man pages, and the other one for the rest;
# we thus need to copy all pages to a temporary directory, and process them there.

# Copy all pages to current dir
cp "${PAGES[@]}" .

for page in "${!PAGES[@]}"; do
stem="${page%.html}"
manpage="${stem%.?}(${stem#*.})"
descr="$(awk -v 'FS=.Nd ' '/.Nd/ { print $2; }' "${PAGES[$page]}")"

	cat >"$1/_documentation/$2/$page" <<EOF
---
layout: doc
title: $manpage [$2]
description: RGBDS $2 — $descr
---
EOF
	options=fragment,man='%N.%S;https://linux.die.net/man/%S/%N'
	if [ $stem = rgbasm.5 ]; then
		options+=,toc
	fi
	mandoc -Thtml -I os=Linux -O$options "${PAGES[$page]##*/}" | .github/actions/doc_postproc.awk >> "$1/_documentation/$2/$page"
	groff -Tpdf -mdoc -wall "${PAGES[$page]##*/}" >"$1/_documentation/$2/$stem.pdf"
	if [ $is_release -ne 0 ]; then
		cat - >"$1/_documentation/$page" <<EOF
---
redirect_to: $WWWPATH/$2/${page%.html}
permalink: $WWWPATH/${page%.html}/
title: $manpage [latest stable]
description: RGBDS latest stable — $descr
---
EOF
	fi
done

cat - >"$1/_documentation/$2/index.html" <<EOF
---
layout: doc_index
permalink: /docs/$2/
title: RGBDS online manual [$2]
description: RGBDS $2 - Online manual
---
EOF


# If making a release, add a new entry right after `master`
if [ $is_release -ne 0 ]; then
	awk '{ print }
/"name": "master"/ { print "\t\t{\"name\": \"'$2'\",  \"text\": \"'$2'\" }," }
' "$1/_data/doc.json" >"$1/_data/doc.json.tmp"
	mv "$1/_data/doc.json"{.tmp,}
fi


# Clean up
rm "${PAGES[@]##*/}"
