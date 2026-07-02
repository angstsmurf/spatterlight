#!/bin/sh
# Build the standalone, deterministic headless SCARE interpreter used to derive
# ADRIFT walkthroughs. Outputs ./scare next to this script.
#
# Usage:  sh build.sh
#
# Notes:
#  - Builds the plain ANSI port (os_ansi.cpp) — reads commands from stdin,
#    prints to stdout. No Glk, links the system zlib (-lz).
#  - Links seed.cpp for deterministic RNG (see seed.cpp).
#  - Does NOT define SPATTERLIGHT (that pulls in glk.h via randomness.h).
#  - The SCARE engine sources were merged into terps/scarier and renamed
#    sc*.c -> sc*.cpp, so we glob sc*.cpp and link with clang++ (linking C++
#    objects with plain `clang` fails on missing std:: symbols).
set -e

# terps/scarier holds the merged SCARE (v4) + a5 (v5) engine. `harness/` is
# two levels below it (terps/scarier/adrift-walkthroughs/harness).
HERE="$(cd "$(dirname "$0")" && pwd)"
SCARE="${SCARE_DIR:-$(cd "$HERE/../.." && pwd)}"
OUT="$HERE/scare"

cd "$SCARE"
clang++ -O2 -w -I. -DSCARE_DUMP_TOOLS \
  sc*.cpp os_ansi.cpp \
  "$HERE/seed.cpp" \
  -lz -o "$OUT"

echo "built: $OUT"
