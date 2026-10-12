#!/bin/sh
# Builds and runs the unit tests of the decompressz80 library, with the address
# and undefined behaviour sanitizers. They make their own snapshots and tapes
# and need no game files.

here=$(cd "$(dirname "$0")" && pwd)
bin="${TMPDIR:-/tmp}/z80_unit"

cc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$here/.." -o "$bin" "$here/z80_unit.c" "$here"/../*.c || exit 1
"$bin"
