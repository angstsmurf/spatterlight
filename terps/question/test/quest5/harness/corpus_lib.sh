# corpus_lib.sh -- what the two golden regressions share: run_replays.sh (the
# native engine against the goldens) and oracle/check_golden.sh (the QuestViva
# oracle against the same goldens). Sourced, not run.
#
# Both walk ../goldens/<Game>.cmd, replay each script against <Game>.quest (or
# .aslx) in $GAMES, and diff the transcript with ../goldens/<Game>.txt. They
# differ only in what does the replaying, so the caller supplies that:
#
#   . corpus_lib.sh                # sets GAMES and GOLDEN, defines the functions
#   run_game() { ... "$1" "$2"; }  # game file, .cmd script -> transcript on stdout
#   export SOMETHING_RUN_GAME_NEEDS  # run_game runs in child shells
#   WORK=<dir>                     # receives <Game>.$SUFFIX.txt, kept for the caller
#   SUFFIX=replay LABEL="native replay" JOBS=<n> DIFF_LINES=<n> FILTERS=("$@")
#   corpus_check                   # prints the table; sets pass, fail, miss, sel
#
# The corpus games are third-party and so untracked: prefer ../games if it has
# been set up (the way quest4/games works) and fall back to ~/Downloads/Quest 5
# games, with GAMES= overriding both.
CORPUS_LIB="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -z "${GAMES:-}" ] && [ -d "$CORPUS_LIB/../games" ]; then GAMES="$CORPUS_LIB/../games"; fi
GAMES="${GAMES:-$HOME/Downloads/Quest 5 games}"
GOLDEN="$CORPUS_LIB/../goldens"

# Each game is an independent process, so both regressions fan out across cores.
default_jobs() { sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4; }

# Optional args restrict a run to matching games (case-insensitive substring on
# the game name), so a single new golden can be re-checked without replaying the
# whole corpus.
FILTERS=()
match_filter() {  # $1 = game name; true if no filters, or any filter matches
  [ "${#FILTERS[@]}" -eq 0 ] && return 0
  local g; g="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
  local f
  for f in "${FILTERS[@]}"; do
    case "$g" in *"$(printf '%s' "$f" | tr '[:upper:]' '[:lower:]')"*) return 0;; esac
  done
  return 1
}

corpus_one() {  # $1 = golden .cmd path; prints one "STATUS<TAB>game<TAB>detail" line
  local cmd="$1" game q gold got
  game="$(basename "$cmd" .cmd)"
  q="$GAMES/$game.quest"; gold="$GOLDEN/$game.txt"
  if [ ! -f "$q" ] && [ -f "$GAMES/$game.aslx" ]; then q="$GAMES/$game.aslx"; fi
  [ -f "$q" ]    || { printf 'MISS\t%s\tgame file missing\n' "$game"; return; }
  [ -f "$gold" ] || { printf 'MISS\t%s\tno golden .txt\n' "$game";    return; }
  got="$WORK/$game.$SUFFIX.txt"
  run_game "$q" "$cmd" > "$got" 2>/dev/null
  if diff -q "$gold" "$got" >/dev/null; then
    printf 'PASS\t%s\t\n' "$game"
  else
    printf 'FAIL\t%s\t%s diff lines\n' "$game" "$(diff "$gold" "$got" | grep -c '^[<>]')"
  fi
}

# Replay every golden the filters select and print one line per game plus the
# totals. DIFF_LINES > 0 also prints that much of each failing game's diff;
# NO_GOLDENS_HINT is appended to the complaint about an empty goldens directory.
corpus_check() {
  local results cmd status game detail
  local sel_list=()
  export -f corpus_one run_game; export GAMES GOLDEN WORK SUFFIX

  results="$(mktemp)"
  for cmd in "$GOLDEN"/*.cmd; do
    [ -e "$cmd" ] || { echo "no goldens in $GOLDEN${NO_GOLDENS_HINT:-}" >&2; rm -f "$results"; exit 2; }
    match_filter "$(basename "$cmd" .cmd)" || continue
    sel_list+=("$cmd")
  done
  sel=${#sel_list[@]}
  if [ "$sel" -gt 0 ]; then
    printf '%s\0' "${sel_list[@]}" | xargs -0 -P "$JOBS" -I{} bash -c 'corpus_one "$@"' _ {} \
      | sort -f -t$'\t' -k2 > "$results"
  fi

  pass=0; fail=0; miss=0
  while IFS=$'\t' read -r status game detail; do
    case "$status" in
      PASS) printf "PASS  %-50s\n" "$game"; pass=$((pass+1));;
      MISS) printf "MISS  %-50s (%s)\n" "$game" "$detail"; miss=$((miss+1));;
      FAIL) printf "FAIL  %-50s (%s)\n" "$game" "$detail"; fail=$((fail+1))
            # The transcripts run to hundreds of KB, so show only the head of
            # the diff: golden on the left ("<"), the replay on the right (">").
            if [ "${DIFF_LINES:-0}" -gt 0 ]; then
              diff "$GOLDEN/$game.txt" "$WORK/$game.$SUFFIX.txt" \
                | head -n "$DIFF_LINES" | sed 's/^/      /'
            fi;;
    esac
  done < "$results"
  rm -f "$results"

  echo "---"
  [ "${#FILTERS[@]}" -gt 0 ] && echo "(filtered: $sel of $(ls "$GOLDEN"/*.cmd | wc -l | tr -d ' ') goldens; ${JOBS}-way parallel)"
  echo "$LABEL: $pass passed, $fail failed, $miss missing"
}
