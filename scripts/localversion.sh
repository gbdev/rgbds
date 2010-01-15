#!/bin/sh

#
# Use git to figure out which version we are using.
#
# Adapted from a script written by Rene Scharfe <rene.scharfe@lsrfire.ath.cx>,
# found in the Linux Kernel.
#

set -u

if head=`git rev-parse --verify --short HEAD 2>/dev/null`; then
	printf '%s%s' -g $head

	git update-index --refresh --unmerged > /dev/null
	if git diff-index --name-only HEAD | read dummy; then
		printf '%s' -dirty
	fi

	exit
fi
