#!/bin/bash
# Golden regression for the qvh oracle. For every driven corpus game, replay the
# FROZEN command script committed under ../../goldens/<Game>.cmd through
# QuestViva and diff the normalised transcript against the committed
# ../../goldens/<Game>.txt.
#
# This deliberately drives from the committed .cmd (not extract_walkthrough.py),
# so a FAIL means the oracle's behaviour drifted — a QuestViva upstream change, a
# .NET/RNG regression, or a Program.cs edit — NOT a walkthrough-extraction tweak.
# (To refresh the goldens after an intended change, run ./update_golden.sh.)
#
# Requires the built oracle (./build.sh) and the corpus games (quest5/games if
# that exists, else ~/Downloads/Quest 5 games; override with GAMES). Exit status
# is non-zero if any game diverges, or if corpus.tsv and the goldens disagree
# about which games are covered (see the coverage cross-check at the bottom).
export PATH="/opt/homebrew/bin:$PATH"
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../corpus_lib.sh"  # GAMES, GOLDEN, match_filter, corpus_check
DLL="$HERE/bin/Release/net10.0/qvh.dll"
WORK="$HERE/out"
mkdir -p "$WORK"

if [ ! -f "$DLL" ]; then
  echo "oracle not built: $DLL missing — run ./build.sh first" >&2
  exit 2
fi

# Optional args restrict the run to matching games (case-insensitive substring on
# the game name), so you can re-check a single new golden without replaying all
# ~68 games:  ./check_golden.sh "lost cavern"   (the coverage cross-check below
# always runs over the full corpus regardless of the filter).
#
# The per-game replay+diff is embarrassingly parallel (each is an independent
# `dotnet` invocation), and dotnet's JIT startup dominates each short game, so we
# fan out across cores instead of looping serially (~68 cold starts -> minutes).
run_game() { dotnet "$DLL" "$1" "$2"; }
export DLL
SUFFIX=golden-check LABEL="golden check" DIFF_LINES=0
NO_GOLDENS_HINT=" — run ./update_golden.sh"
JOBS="${GOLDEN_JOBS:-$(default_jobs)}"
FILTERS=("$@")
corpus_check

# Coverage cross-check. corpus_check iterates ../../goldens/*.cmd, so it only
# ever visits games that ALREADY have a golden: a corpus row that is driven but
# was never frozen into goldens/ is not regression-checked and nothing complains.
# (run_corpus.sh's counterpart guard catches the other half — a corpus row that
# cannot be driven at all.) Compare the two directions explicitly.
rows="$(mktemp)"; trap 'rm -f "$rows"' EXIT
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  [ "$mode" = "hints" ] || printf '%s\n' "$game"
done < "$HERE/corpus.tsv" > "$rows"

ungolden=0; orphan=0
while IFS= read -r game; do
  if [ ! -f "$GOLDEN/$game.cmd" ] || [ ! -f "$GOLDEN/$game.txt" ]; then
    printf "NOGOLD  %-48s (driven corpus row, but no frozen golden)\n" "$game"
    ungolden=$((ungolden+1))
  fi
done < "$rows"

for cmd in "$GOLDEN"/*.cmd; do
  [ -e "$cmd" ] || continue
  game="$(basename "$cmd" .cmd)"
  if ! grep -Fxq "$game" "$rows"; then
    printf "ORPHAN  %-48s (golden with no corpus.tsv row — stale?)\n" "$game"
    orphan=$((orphan+1))
  fi
done

echo "coverage: $(wc -l < "$rows" | tr -d ' ') driven corpus rows, $ungolden without a golden, $orphan orphaned golden(s)"

# Corpus composition. These are the numbers README.md used to hard-code, and they
# went stale every time a game was wired (each per-game commit touches corpus.tsv,
# the goldens and overrides/README.md, but not the README totals). Derive them here
# instead so the source of truth is the manifest, not prose. Like the coverage
# cross-check above, this always covers the FULL corpus, ignoring any filter args.
wt_rows=0; ov_rows=0; ov_driven=0; extracted=0
fin=0; runn=0; wedge=0; unknown=0
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  [ "$mode" = "hints" ] && continue
  # wt="-" = no published walkthrough anywhere; the override IS the script.
  if [ "$wt" = "-" ]; then ov_rows=$((ov_rows+1)); else wt_rows=$((wt_rows+1)); fi
  if [ -f "$HERE/overrides/$game.cmd" ]; then ov_driven=$((ov_driven+1)); else extracted=$((extracted+1)); fi
  # Final state comes from the FROZEN golden, not this run's replay: the golden is
  # the committed answer key, and a FAIL above already reports any live divergence.
  case "$(tail -1 "$GOLDEN/$game.txt" 2>/dev/null)" in
    '[state=Finished]') fin=$((fin+1));;
    '[state=Running]')  runn=$((runn+1));;
    '[state=Wedged]')   wedge=$((wedge+1));;
    *)                  unknown=$((unknown+1));;
  esac
done < "$HERE/corpus.tsv"
ov_wt=$((ov_driven - ov_rows))   # overrides on rows that DO have a walkthrough
echo "composition: $((wt_rows + ov_rows)) driven rows = $wt_rows walkthrough-mapped + $ov_rows override-only"
echo "             $ov_driven driven by curated overrides ($ov_rows override-only + $ov_wt walkthrough rows), $extracted via extract_walkthrough.py"
printf "final states (frozen goldens/*.txt): %d Finished, %d Running, %d Wedged" "$fin" "$runn" "$wedge"
[ "$unknown" -gt 0 ] && printf ", %d with no readable [state=] line" "$unknown"
echo

[ "$fail" -eq 0 ] && [ "$ungolden" -eq 0 ] && [ "$orphan" -eq 0 ]
