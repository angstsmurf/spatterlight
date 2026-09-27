#!/bin/bash
# Refresh the committed golden baseline after an INTENDED change (a QuestViva
# bump, an extractor improvement, a Program.cs output tweak). Runs the full
# corpus pipeline (extract walkthroughs -> drive -> transcript) and copies the
# resulting out/<Game>.cmd + out/<Game>.txt for every driven game into
# ../../goldens/.
#
# Review the git diff of the goldens before committing — that diff IS the change
# in what "real Quest does" for the affected games.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
GOLDEN="$HERE/../../goldens"
# Deliberately tolerant: a partial run should still refresh the goldens it did
# produce. But do not lose run_corpus.sh's non-zero exit — that means a corpus
# row could not be driven at all, and refreshing around it would bake the gap in.
"$HERE/run_corpus.sh"; corpus_rc=$?   # regenerates out/*.cmd and out/*.txt
mkdir -p "$GOLDEN"
n=0
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  [ "$mode" = "hints" ] && continue
  if [ -f "$HERE/out/$game.txt" ] && [ -f "$HERE/out/$game.cmd" ]; then
    cp "$HERE/out/$game.txt" "$GOLDEN/$game.txt"
    cp "$HERE/out/$game.cmd" "$GOLDEN/$game.cmd"
    n=$((n+1))
  else
    echo "WARN: no transcript for $game (not refreshed)" >&2
  fi
done < "$HERE/corpus.tsv"
echo "refreshed $n golden transcripts in quest5/goldens/"
if [ "$corpus_rc" -ne 0 ]; then
  echo "ERROR: run_corpus.sh failed — some corpus row(s) were not driven and so" >&2
  echo "       were NOT refreshed above. Fix those rows before committing goldens/." >&2
  exit 1
fi
