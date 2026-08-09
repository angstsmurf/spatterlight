#!/bin/sh
# ADRIFT 5 auto-map raster regression, over the hand-authored stress test
# test/adrift5/probes/src/map.xml (spec: ADRIFT-5-XML.md section 6.15).  Renders a
# fixed set of views with test/adrift5/harness/a5map_dump and compares each PPM's sha256
# against the committed manifest test/adrift5/probes/map_golden.sha256.
#
# usage: test/adrift5/harness/run_a5_maptest.sh [-b]
#   -b   bless: overwrite the manifest with the current hashes
#
# The hashes are SELF-blessed (the real run500.exe map is GUI-only, so there
# is no headless ground truth to diff against): they guard against renderer
# regressions, not conformance.  On a mismatch, eyeball the change first:
#   sips -s format png /tmp/a5maptest/<view>.ppm --out /tmp/<view>.png
# and re-bless with -b only if the new rendering is intentional.
#
# The hashes survive `make a5probetafs` rebuilds of map.taf: the taf's random
# IFID is not rendered.

HERE=$(cd "$(dirname "$0")/../../.." && pwd)
BIN="$HERE/test/adrift5/harness/a5map_dump"
TAF="$HERE/test/adrift5/probes/map.taf"
MANIFEST="$HERE/test/adrift5/probes/map_golden.sha256"
OUT=/tmp/a5maptest

BLESS=0
[ "$1" = "-b" ] && BLESS=1

if [ ! -x "$BIN" ]; then
    echo "run_a5_maptest: $BIN missing -- build it with:" >&2
    echo "  make -f Makefile.headless a5map" >&2
    exit 2
fi

mkdir -p "$OUT"
: > "$OUT/manifest"

# view name | extra a5map_dump args | commands fed as the script
render() {
    name=$1; args=$2
    script="$OUT/$name.script"
    cat > "$script"
    # shellcheck disable=SC2086
    "$BIN" "$TAF" "$script" -o "$OUT/$name.ppm" $args >/dev/null 2>&1 || {
        echo "run_a5_maptest: render failed for view '$name'" >&2
        exit 2
    }
    hash=$(shasum -a 256 "$OUT/$name.ppm" | cut -d' ' -f1)
    printf '%s  %s\n' "$hash" "$name" >> "$OUT/manifest"
}

# Fresh start: only Hub seen.
render start   "" </dev/null
# Main-page reveal: duplex/bent/one-way connectors, dotted Cellar-East
# (blocked once during the reveal), solid Wide-South, stubs to unseen rooms,
# dimmed Attic, hidden node omitted.
render reveal  "" <<'EOF'
reveal
EOF
# Link-toggle states: Cellar-East and Wide-South omitted while closed,
# gate flips the NW/SW off-page stubs.
render flips   "" <<'EOF'
reveal
close door
close latch
open gate
EOF
# Z=1 focus: Z=0 boxes dim.
render attic   "" <<'EOF'
reveal
attic
EOF
# Second map page, reached via the Annex teleport tasks.
render annex   "" <<'EOF'
reveal annex
annex
EOF
# Whole-map development view.
render allseen "-all" </dev/null

if [ "$BLESS" = 1 ]; then
    cp "$OUT/manifest" "$MANIFEST"
    echo "blessed $(wc -l < "$MANIFEST" | tr -d ' ') view hashes into ${MANIFEST#$HERE/}"
    exit 0
fi

if [ ! -f "$MANIFEST" ]; then
    echo "run_a5_maptest: no manifest $MANIFEST -- bless one with -b" >&2
    exit 2
fi

if diff -u "$MANIFEST" "$OUT/manifest"; then
    echo "a5maptest: $(wc -l < "$MANIFEST" | tr -d ' ') views match"
else
    echo "a5maptest: FAIL -- renders differ from ${MANIFEST#$HERE/}" >&2
    echo "eyeball with: sips -s format png $OUT/<view>.ppm --out /tmp/<view>.png" >&2
    exit 1
fi
