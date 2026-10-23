#!/bin/sh
# Differential check of the Graphics Magician opcode interpreters.
#   test/gm_stream_diff.sh <baseline-git-ref> [cases] [seed]
# Builds test/gm_stream_dump.cpp once per renderer from <baseline-git-ref> (in a
# temporary worktree) and once from the working tree, runs both and diffs the
# output. Needs no game fixtures. Run from terps/comprehend.
set -e
BASE=${1:?usage: $0 <baseline-git-ref> [cases] [seed]}
CASES=${2:-300}
SEED=${3:-1}
OUT=${GM_DIFF_OUT:-/tmp/gm_stream_diff}
CXX=${CXX:-g++}
# GM_DIFF_SAN=-fsanitize=address to debug a harness stream; off by default because
# the legacy Apple fill reads one row past its page (baseline behaviour).
GM_DIFF_SAN=${GM_DIFF_SAN:-}
HERE=$(pwd)
TOP=$(git rev-parse --show-toplevel)
REL=${HERE#$TOP/}
rm -rf "$OUT"; mkdir -p "$OUT"
git worktree add -f --detach "$OUT/base" "$BASE" >/dev/null 2>&1
trap 'git worktree remove --force "$OUT/base" >/dev/null 2>&1 || true' EXIT

build() { # dir renderer-macro sources...
	dir=$1; mac=$2; exe=$3; shift 3
	(cd "$dir" && $CXX -std=c++14 -O1 -g -w $GM_DIFF_SAN -I. -I../common_sagadraw \
		-D$mac -o "$exe" "$HERE/test/gm_stream_dump.cpp" "$@")
}
SAGA="../common_sagadraw/gm_vector.c ../common_sagadraw/gm_artifact.c"
fail=0
for r in APPLE DHGR CGA PCJR; do
	case $r in
	APPLE) srcs="graphics_magician.cpp $SAGA" ;;
	DHGR)  srcs="graphics_magician_dhgr.cpp graphics_magician.cpp $SAGA" ;;
	CGA)   srcs="graphics_magician_cga.cpp" ;;
	PCJR)  srcs="graphics_magician_pcjr.cpp" ;;
	esac
	build "$OUT/base/$REL" GMD_$r "$OUT/base_$r" $srcs
	build "$HERE" GMD_$r "$OUT/new_$r" $srcs
	"$OUT/base_$r" "$CASES" "$SEED" > "$OUT/base_$r.txt"
	"$OUT/new_$r"  "$CASES" "$SEED" > "$OUT/new_$r.txt"
	if cmp -s "$OUT/base_$r.txt" "$OUT/new_$r.txt"; then
		echo "OK   $r ($(wc -l < "$OUT/new_$r.txt") cases)"
	else
		echo "DIFF $r: $(diff "$OUT/base_$r.txt" "$OUT/new_$r.txt" | grep -c '^<') cases differ"
		fail=1
	fi
done
exit $fail
