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
# NB: the dump/trace instrumentation (scdump.cpp, sctasks.cpp) is guarded by
# SCARIER_DUMP_TOOLS -- the pre-rename name was SCARE_DUMP_TOOLS, which no longer
# matches anything, so defining that left the tools compiled out (dead code).
# The instrumentation is entirely env-var-gated (SCR_DUMP_TASKS, SCR_TRACE_*,
# etc.), so with those vars unset the binary is behaviourally identical to a
# build without it -- goldens are unaffected -- but SCR_DUMP_TASKS now works for
# route debugging (e.g. finding a win task's command + restrictions).
clang++ -O2 -w -I. -DSCARIER_DUMP_TOOLS \
  sc*.cpp os_ansi.cpp \
  "$HERE/seed.cpp" \
  -lz -o "$OUT"

echo "built: $OUT"
