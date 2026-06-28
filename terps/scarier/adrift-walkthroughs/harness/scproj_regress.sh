#!/bin/sh
# Deterministic projectile-combat regression for the SCARE Battle System port.
#
#   sh scproj_regress.sh           # build + run + diff against the golden file
#   sh scproj_regress.sh --bless   # regenerate the golden file
#
# Builds scproj_test.c against the SCARE sources WITH seed.c (fixed RNG), so
# combat outcomes are reproducible. For each game it arms the player with that
# game's projectile weapon, co-locates an enemy, and resolves a fight through
# the real battle_player_attack() pipeline.
#
# Coverage:
#   deaths.taf   v3.90  shoot (method 3 = replaces strength) + legacy hit model
#                       (all-zero acc/agi yet the hit lands)
#   Colony.taf   v3.90  shoot, hit=200 one-shot, legacy model
#   light_up...  v4.00  throw (method 5 = adds HitValue) + KilledTask cutscene
#   Space Boy's  v4.00  shoot + the v4.0 acc>agi hit model (RNG damage ranges)
#   cyber2.taf   v4.00  faithful zero-accuracy stalemate (enemy always dodges;
#                       combat-assist correctly does NOT rescue a real v4.0 game)
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
SCARE="${SCARE_DIR:-/Users/administrator/spatterlight/terps/scare}"
GAMES="${GAMES_DIR:-$HERE/../games}"
GOLDEN="$HERE/scproj_regress.golden"
BIN="$HERE/scproj_det"

GAME_LIST="deaths.taf
Colony.taf
light_up_4summer_comp.taf
Space Boy's First Adventure.taf
cyber2.taf"

# Build the deterministic harness (sx* stubs supply the os-layer callbacks;
# seed.c forces SCARE's portable RNG + fixed seed before main()).
( cd "$SCARE" && clang -O2 -w -I. sc*.c sxstubs.c sxglob.c sxutils.c \
    "$HERE/scproj_test.c" "$HERE/seed.c" -lz -o "$BIN" )

run_all() {
  echo "$GAME_LIST" | while IFS= read -r g; do
    [ -n "$g" ] || continue
    "$BIN" "$GAMES/$g"
  done
}

if [ "$1" = "--bless" ]; then
  run_all > "$GOLDEN"
  echo "blessed: $GOLDEN"
  exit 0
fi

if [ ! -f "$GOLDEN" ]; then
  echo "no golden file; run: sh $0 --bless" >&2
  exit 2
fi
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
run_all > "$TMP"
if diff -u "$GOLDEN" "$TMP" ; then
  echo "PASS: projectile combat matches golden"
else
  echo "FAIL: projectile combat diverged from golden" >&2
  exit 1
fi
