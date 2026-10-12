#!/bin/sh
# Builds and runs the woz2nib unit tests, with the address and undefined
# behaviour sanitizers. They make their own bitstreams and need no images.

here=$(cd "$(dirname "$0")" && pwd)
terps=$(cd "$here/../.." && pwd)
bin="${TMPDIR:-/tmp}/woz2nib_unit"

cc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$here/.." -I"$terps/common_utils" -o "$bin" "$here/woz2nib_unit.c" || exit 1
"$bin"
