#!/bin/sh
# Deterministic projectile-combat regression for the SCARE Battle System port.
#
#   sh scproj_regress.sh           # build + run + diff against the golden file
#   sh scproj_regress.sh --bless   # regenerate the golden file
#
# Builds scproj_test.cpp against the SCARIER sources WITH seed.cpp (fixed RNG),
# so combat outcomes are reproducible. For each game it arms the player with
# that game's projectile weapon, co-locates an enemy, and resolves a fight
# through the real battle_player_attack() pipeline.
#
# The 5 games below are third-party data and are NOT committed (same policy as
# the walkthrough games): they are found in GAMES_DIR or the ALT_DIRS fallback.
# If any is absent the run SKIPs (exit 0) rather than failing.
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
# The SCARE (v4) engine was merged into terps/scarier; harness/ is two levels
# below it (terps/scarier/adrift-walkthroughs/harness).
SCARE="${SCARE_DIR:-$(cd "$HERE/../.." && pwd)}"
GAMES="${GAMES_DIR:-$HERE/../games}"
# Extra dirs searched (by basename) when a game isn't in GAMES.
ALT_DIRS="$HOME/adrift-battle/games"
GOLDEN="$HERE/scproj_regress.golden"
BIN="$HERE/scproj_det"

GAME_LIST="deaths.taf
Colony.taf
light_up_4summer_comp.taf
Space Boy's First Adventure.taf
cyber2.taf"

find_game() {  # $1=basename -> prints path or nothing
  if [ -f "$GAMES/$1" ]; then printf '%s\n' "$GAMES/$1"; return; fi
  for d in $ALT_DIRS; do
    [ -f "$d/$1" ] && { printf '%s\n' "$d/$1"; return; }
  done
}

# Resolve all games up front; SKIP the whole run if any is missing (the golden
# is a single concatenated transcript, so a partial run can't match it).
missing=""
while IFS= read -r g; do
  [ -n "$g" ] || continue
  [ -n "$(find_game "$g")" ] || missing="$missing $g"
done <<EOF
$GAME_LIST
EOF
if [ -n "$missing" ]; then
  echo "SKIP: battle games not found (drop into $GAMES or \$HOME/adrift-battle/games):$missing" >&2
  exit 0
fi

# Build the deterministic harness (sx* stubs supply the os-layer callbacks;
# seed.cpp forces SCARIER's portable RNG + fixed seed before main()).  Engine
# ported C->C++ (sc*.c -> sc*.cpp), so glob the .cpp sources and use clang++.
# mapdraw.cpp is not matched by the sc*.cpp glob, but scmap.cpp (the ADRIFT 4
# map port) calls map_build()/map_free() from it, so the link needs it -- same
# addition build.sh makes.
( cd "$SCARE" && clang++ -std=c++11 -O2 -w -I. sc*.cpp mapdraw.cpp sxstubs.cpp sxglob.cpp sxutils.cpp \
    "$HERE/scproj_test.cpp" "$HERE/seed.cpp" -lz -o "$BIN" )

run_all() {
  echo "$GAME_LIST" | while IFS= read -r g; do
    [ -n "$g" ] || continue
    "$BIN" "$(find_game "$g")"
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
