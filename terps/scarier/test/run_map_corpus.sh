#!/bin/sh
# Whole-corpus map raster regression, both engines.  Renders the player's-room
# view and the -all development view of every game in test/adrift4/games/ and
# test/adrift5/games/ and compares each PPM's sha256 against the committed
# manifest test/map_corpus_golden.txt.  This is the map counterpart of the
# walkthrough goldens: run it after touching scmap.cpp, adrift5/a5map.cpp or
# mapdraw.cpp -- a behaviour-preserving change must leave every hash alone.
#
# usage: test/run_map_corpus.sh [-b]
#   -b   bless: overwrite the manifest with the current hashes
#
# Manifest lines are "<engine> <game> <view> <sha256|SKIP>", tab-separated
# (58 of the game filenames contain spaces).  SKIP is itself
# an assertion: that game/view renders no map at all (no map data, a hidden
# start room, or an ADRIFT 4 layout the Form29 algorithm gives up on), and a
# change that makes one appear is flagged like any other diff.
#
# The corpora are untracked (see test/fetch_games.sh); games listed in the
# manifest but absent on disk are reported and skipped, not failed, so a
# partial fetch still checks what it can.
#
# Like the a5 probe manifest (test/adrift5/probes/map_golden.sha256) these
# hashes are self-blessed -- the real Runners' maps are GUI-only -- so they
# guard against renderer regressions, not conformance.  On a mismatch,
# eyeball the change first:
#   sips -s format png /tmp/mapcorpus/<game>.<view>.ppm --out /tmp/x.png
# and re-bless with -b only if the new rendering is intentional.

HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN4="$HERE/test/adrift4/harness/scmap_dump"
BIN5="$HERE/test/adrift5/harness/a5map_dump"
MANIFEST="$HERE/test/map_corpus_golden.txt"
OUT=/tmp/mapcorpus

BLESS=0
[ "$1" = "-b" ] && BLESS=1

missing_bin=0
for bin in "$BIN4" "$BIN5"; do
    if [ ! -x "$bin" ]; then
        echo "run_map_corpus: $bin missing" >&2
        missing_bin=1
    fi
done
if [ "$missing_bin" = 1 ]; then
    echo "build both with:  make -f Makefile.headless scmap a5map" >&2
    exit 2
fi

mkdir -p "$OUT"
: > "$OUT/manifest"

# engine tag | dump binary | game path
render_views() {
    engine=$1 bin=$2 game=$3
    name=$(basename "$game")
    for view in start all; do
        args=""
        [ "$view" = all ] && args="-all"
        ppm="$OUT/$name.$view.ppm"
        rm -f "$ppm"
        # shellcheck disable=SC2086
        if "$bin" "$game" -o "$ppm" $args >/dev/null 2>&1 && [ -s "$ppm" ]; then
            hash=$(shasum -a 256 "$ppm" | cut -d' ' -f1)
        else
            hash=SKIP
        fi
        printf '%s\t%s\t%s\t%s\n' "$engine" "$name" "$view" "$hash" \
            >> "$OUT/manifest"
    done
}

# Match the extension case-insensitively: a handful of corpus games are named
# .TAF, and a plain *.taf glob rendered none of them.
for g in "$HERE"/test/adrift4/games/*; do
    [ -e "$g" ] || continue
    [ "$(printf '%s' "${g##*.}" | tr 'A-Z' 'a-z')" = taf ] || continue
    render_views v4 "$BIN4" "$g"
done
for g in "$HERE"/test/adrift5/games/*; do
    [ -e "$g" ] && render_views a5 "$BIN5" "$g"
done

total=$(wc -l < "$OUT/manifest" | tr -d ' ')
if [ "$total" = 0 ]; then
    echo "run_map_corpus: no games found -- fetch them with:" >&2
    echo "  sh test/fetch_games.sh fetch adrift4" >&2
    echo "  sh test/fetch_games.sh fetch adrift5" >&2
    exit 2
fi

if [ "$BLESS" = 1 ]; then
    cp "$OUT/manifest" "$MANIFEST"
    echo "blessed $total view hashes into ${MANIFEST#$HERE/}"
    exit 0
fi

if [ ! -f "$MANIFEST" ]; then
    echo "run_map_corpus: no manifest $MANIFEST -- bless one with -b" >&2
    exit 2
fi

# Compare only the manifest rows whose game is on disk (the corpora are
# fetched, so a fresh clone may hold a subset); count the rest as unfetched.
awk -F'\t' 'NR==FNR { here[$1 FS $2] = 1; next } ($1 FS $2) in here' \
    "$OUT/manifest" "$MANIFEST" > "$OUT/expected"
unfetched=$(( $(wc -l < "$MANIFEST") - $(wc -l < "$OUT/expected") ))

if diff -u "$OUT/expected" "$OUT/manifest"; then
    checked=$(wc -l < "$OUT/expected" | tr -d ' ')
    msg="mapcorpus: $checked views match"
    [ "$unfetched" -gt 0 ] && msg="$msg ($unfetched not checked: game not fetched)"
    echo "$msg"
else
    echo "mapcorpus: FAIL -- renders differ from ${MANIFEST#$HERE/}" >&2
    echo "eyeball with: sips -s format png $OUT/<game>.<view>.ppm --out /tmp/x.png" >&2
    exit 1
fi
