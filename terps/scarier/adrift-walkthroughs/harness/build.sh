#!/bin/sh
# Build the standalone, deterministic headless SCARE interpreter used to derive
# ADRIFT walkthroughs. Outputs ./scare next to this script.
#
# Usage:  sh build.sh
#
# Notes:
#  - Builds the plain ANSI port (os_ansi.c) — reads commands from stdin, prints
#    to stdout. No Glk, links the system zlib (-lz).
#  - Links seed.c for deterministic RNG (see seed.c).
#  - Does NOT define SPATTERLIGHT (that pulls in glk.h via randomness.h).
set -e

SCARE="${SCARE_DIR:-/Users/administrator/spatterlight/terps/scare}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/scare"

cd "$SCARE"
clang -O2 -w -I. -DSCARE_DUMP_TOOLS \
  sctafpar.c sctaffil.c scprops.c scvars.c scexpr.c scprintf.c scinterf.c \
  scparser.c sclibrar.c scrunner.c scevents.c scnpcs.c scbattle.c scobjcts.c \
  sctasks.c screstrs.c scgamest.c scserial.c scresour.c scmemos.c scutils.c \
  sclocale.c scdebug.c scdump.c os_ansi.c \
  "$HERE/seed.c" \
  -lz -o "$OUT"

echo "built: $OUT"
