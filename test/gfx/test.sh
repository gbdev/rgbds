#!/usr/bin/env bash

[[ -e ./rgbgfx_test ]] || make -C ../.. test/gfx/rgbgfx_test
[[ -e ./randtilegen ]] || make -C ../.. test/gfx/randtilegen

rc=0
for f in *.bin; do
	printf '%s...\n' "$f"
	./rgbgfx_test "$f" || rc=$?
done

exit $rc
