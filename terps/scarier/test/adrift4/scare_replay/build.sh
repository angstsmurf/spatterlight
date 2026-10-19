#!/bin/sh
# Build stock SCARE 1.3.10 with the replay patch into ./build/scare.
# The source is Spatterlight's own copy, taken from Gargoyle in ed31bf462.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
B="$HERE/build"
rm -rf "$B"; mkdir -p "$B"
git -C "$(git -C "$HERE" rev-parse --show-toplevel)" archive ed31bf462:terps/scare | tar -x -C "$B"
( cd "$B" && patch -s -p1 < "$HERE/scare1310.patch" )
( cd "$B" && ${CC:-cc} -O2 -w -o scare \
    sctafpar.c sctaffil.c scprops.c scvars.c scexpr.c scprintf.c scinterf.c \
    scparser.c sclibrar.c scrunner.c scevents.c scnpcs.c scobjcts.c sctasks.c \
    screstrs.c scgamest.c scserial.c scresour.c scmemos.c scutils.c \
    sclocale.c scdebug.c os_ansi.c -lz )
echo "$B/scare"
