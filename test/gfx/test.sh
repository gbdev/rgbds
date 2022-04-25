#!/usr/bin/env bash

[[ -e ./rgbgfx_test ]] || make -C ../.. test/gfx/rgbgfx_test
[[ -e ./randtilegen ]] || make -C ../.. test/gfx/randtilegen

rc=0
for f in *.bin; do
	./rgbgfx_test "$f" || rc=1
done

exit $rc
