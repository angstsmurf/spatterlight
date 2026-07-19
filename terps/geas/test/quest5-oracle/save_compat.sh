#!/usr/bin/env bash
# save_compat.sh -- native <-> QuestViva `.quest-save` compatibility cross-test.
#
# For each game, drive the first half of its golden walkthrough, snapshot a
# native save, then continue the second half in the OTHER engine from that save
# and check the continuation transcript matches the pure-QuestViva baseline.
#
#   baseline  : qvh first-half (save qv.qsave); qvh loads qv.qsave, runs 2nd half  -> T_oracle
#   reader    : qvh writes qv.qsave;   native loads it, runs 2nd half              -> compare to T_oracle
#   writer    : native writes na.qsave; qvh loads it,   runs 2nd half              -> compare to T_oracle
#
# The reader leg proves we load a save QuestViva WROTE; the writer leg proves
# QuestViva loads a save WE wrote. Both are compared to the same oracle baseline,
# so no per-game state assertions are needed.
#
# Known benign artifacts (transcript, not state -- these make a game "FAIL" the
# strict diff without meaning a save-compat bug):
#   * pre-v540 games: QuestViva embeds the HTML output-log in a save and its
#     legacy logger REPLAYS the whole pre-save transcript on restore. Our engine
#     does not carry that log, so our leg lacks the replayed prefix (the oracle
#     baseline has it). Game state still restores correctly.
#   * object-list display ORDER can differ by a sort_index round-trip nuance
#     after a runtime MoveObject (Basilica de Sangre): cosmetic, state is intact.
# ~18 diverse games (incl. Dracula, Mouse, Bony King, and the grid-map games
# Night House / Poppet / Jacqueline) verify byte-identical BOTH directions.
#
# Usage: ./save_compat.sh [Game ...]   (default: a small, menu-free set)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
GAMES="${GAMES:-$HOME/Downloads/Quest 5 games}"
QVH="dotnet $HERE/bin/Release/net10.0/qvh.dll"
REPLAY="$HERE/../aslx_replay"
CORE="$HERE/../../quest5/aslx-core"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

DEFAULT_GAMES=("Dream Pieces" "One Night Stand" "A Hobbit Trek")
GAMELIST=("$@")
[ ${#GAMELIST[@]} -eq 0 ] && GAMELIST=("${DEFAULT_GAMES[@]}")

pass=0; fail=0
for game in "${GAMELIST[@]}"; do
    quest="$GAMES/$game.quest"
    cmd="$HERE/golden/$game.cmd"
    if [ ! -f "$quest" ] || [ ! -f "$cmd" ]; then
        echo "SKIP  $game (missing game or golden)"; continue
    fi

    # Split executable steps (drop comments/blanks) at the midpoint.
    grep -vE '^[[:space:]]*#|^[[:space:]]*$' "$cmd" > "$TMP/steps.cmd"
    n=$(wc -l < "$TMP/steps.cmd" | tr -d ' ')
    mid=$(( n / 2 ))
    h1="$TMP/h1.cmd"; h2="$TMP/h2.cmd"
    head -n "$mid" "$TMP/steps.cmd" > "$h1"
    tail -n "+$((mid + 1))" "$TMP/steps.cmd" > "$h2"

    # --- baseline: pure-QuestViva save + continue ------------------------------
    cp "$h1" "$TMP/h1qv.cmd"; echo "save:$TMP/qv.qsave" >> "$TMP/h1qv.cmd"
    $QVH "$quest" "$TMP/h1qv.cmd" >/dev/null 2>"$TMP/h1qv.err" || { echo "FAIL  $game (qvh half1 crashed)"; fail=$((fail+1)); continue; }
    if [ ! -s "$TMP/qv.qsave" ]; then echo "FAIL  $game (qvh produced no save)"; fail=$((fail+1)); continue; fi
    QVH_LOADSAVE="$TMP/qv.qsave" $QVH "$quest" "$h2" >"$TMP/oracle.out" 2>"$TMP/oracle.err" \
        || { echo "FAIL  $game (qvh continue crashed)"; fail=$((fail+1)); continue; }

    # --- reader: native loads the QuestViva save, continues --------------------
    ASLX_CORE="$CORE" ASLX_RESTORE="$TMP/qv.qsave" "$REPLAY" "$quest" "$h2" \
        >"$TMP/reader.out" 2>"$TMP/reader.err"

    # --- writer: native writes a save, QuestViva continues from it -------------
    cp "$h1" "$TMP/h1na.cmd"; echo "save:$TMP/na.qsave" >> "$TMP/h1na.cmd"
    ASLX_CORE="$CORE" "$REPLAY" "$quest" "$TMP/h1na.cmd" >/dev/null 2>"$TMP/h1na.err"
    if [ ! -s "$TMP/na.qsave" ]; then echo "FAIL  $game (native produced no save)"; fail=$((fail+1)); continue; fi
    QVH_LOADSAVE="$TMP/na.qsave" $QVH "$quest" "$h2" >"$TMP/writer.out" 2>"$TMP/writer.err" \
        || { echo "FAIL  $game (qvh load-native crashed; see $TMP)"; cp -r "$TMP" "$TMP-$game" 2>/dev/null; fail=$((fail+1)); continue; }

    # Normalise away the "Loaded saved game" line: QuestViva prints it when a
    # save carries no HTML output-log (a transcript-pane presentation detail we
    # deliberately omit -- game STATE is fully restored). It is not a state
    # divergence, so strip it from every transcript before comparing.
    for f in oracle reader writer; do
        grep -v '^Loaded saved game$' "$TMP/$f.out" > "$TMP/$f.norm"
    done

    r_ok=no; w_ok=no
    diff -q "$TMP/oracle.norm" "$TMP/reader.norm" >/dev/null 2>&1 && r_ok=yes
    diff -q "$TMP/oracle.norm" "$TMP/writer.norm" >/dev/null 2>&1 && w_ok=yes

    if [ "$r_ok" = yes ] && [ "$w_ok" = yes ]; then
        echo "PASS  $game (reader+writer, ${n} steps split at ${mid})"
        pass=$((pass+1))
    else
        echo "FAIL  $game (reader=$r_ok writer=$w_ok)"
        keep="$HERE/_savecompat_$game"; rm -rf "$keep"; cp -r "$TMP" "$keep"
        echo "      artifacts kept in $keep"
        fail=$((fail+1))
    fi
done

echo "----"
echo "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
