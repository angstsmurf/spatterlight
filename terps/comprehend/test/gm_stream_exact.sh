#!/bin/sh
# Truncated-input check for the Graphics Magician opcode interpreters:
#   test/gm_stream_exact.sh [cases] [seed]
# Renders random streams, a sixth of them cut 1-2 bytes short, each in a heap
# block of exactly its length under AddressSanitizer, through every renderer.
# Any operand read past the end of a stream aborts with a report.  Needs no game
# fixtures.  Run from terps/comprehend.
set -e
CASES=${1:-150}
SEED=${2:-3}
OUT=${GM_DIFF_OUT:-/tmp/gm_stream_exact}
CXX=${CXX:-g++}
rm -rf "$OUT"; mkdir -p "$OUT"
SAGA="../common_sagadraw/gm_vector.c ../common_sagadraw/gm_artifact.c"
for r in APPLE DHGR CGA PCJR; do
	case $r in
	APPLE) srcs="graphics_magician.cpp $SAGA" ;;
	DHGR)  srcs="graphics_magician_dhgr.cpp graphics_magician.cpp $SAGA" ;;
	CGA)   srcs="graphics_magician_cga.cpp" ;;
	PCJR)  srcs="graphics_magician_pcjr.cpp" ;;
	esac
	$CXX -std=c++14 -O1 -g -w -fsanitize=address,undefined -fno-sanitize-recover=all -I. -I../common_sagadraw \
		-DGMD_$r -o "$OUT/exact_$r" test/gm_stream_dump.cpp $srcs
	GMD_EXACT=1 "$OUT/exact_$r" "$CASES" "$SEED" > "$OUT/exact_$r.txt"
	echo "OK   $r ($(wc -l < "$OUT/exact_$r.txt") cases, no over-read)"
done
