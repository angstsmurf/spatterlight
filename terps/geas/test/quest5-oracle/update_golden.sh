#!/bin/bash
# Refresh the committed golden baseline after an INTENDED change (a QuestViva
# bump, an extractor improvement, a Program.cs output tweak). Runs the full
# corpus pipeline (extract walkthroughs -> drive -> transcript) and copies the
# resulting out/<Game>.cmd + out/<Game>.out for every driven game into golden/.
#
# Review the git diff of golden/ before committing — that diff IS the change in
# what "real Quest does" for the affected games.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
"$HERE/run_corpus.sh" || true   # regenerates out/*.cmd and out/*.out
mkdir -p "$HERE/golden"
n=0
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  [ "$mode" = "hints" ] && continue
  if [ -f "$HERE/out/$game.out" ] && [ -f "$HERE/out/$game.cmd" ]; then
    cp "$HERE/out/$game.out" "$HERE/golden/$game.out"
    cp "$HERE/out/$game.cmd" "$HERE/golden/$game.cmd"
    n=$((n+1))
  else
    echo "WARN: no transcript for $game (not refreshed)" >&2
  fi
done < "$HERE/corpus.tsv"
echo "refreshed $n golden transcripts in golden/"
