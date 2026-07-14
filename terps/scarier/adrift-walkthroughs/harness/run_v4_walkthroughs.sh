#!/bin/sh
# Deterministic ADRIFT-3.9/4.0 (SCARE-engine) walkthrough regression, modelled
# on test/run_a5_walkthroughs.sh (which does the same job for the ADRIFT-5 a5
# engine).  For each (solution, game) pair it runs the seeded headless `scare`
# binary over the solution script and strict-diffs the transcript against a
# committed golden.  A golden MATCH is the pass; an optional per-row win marker
# guards against a silently-desynced walkthrough being blessed as "passing"
# (see TODO_plover_walkthroughs.md §6/§7 -- Key & Compass scripts desync on the
# games' interactive "(Press a key)" pauses).  [TODO_plover_walkthroughs.md was
# pruned 2026-07-14 once every item closed; citations to it here and in the
# *_walkthrough.md files resolve via git history:
#   git log --all -- terps/scarier/adrift-walkthroughs/TODO_plover_walkthroughs.md]
#
# Usage:
#   sh run_v4_walkthroughs.sh [substring]        # run + diff, table + exit code
#   sh run_v4_walkthroughs.sh --bless [substring] # (re)generate goldens
#   sh run_v4_walkthroughs.sh -v [substring]      # dump each failing diff
#
# A solution's golden is  harness/<solution-basename-sans-.txt>.expected.txt.
# Game .taf files are third-party data and are NOT committed (same policy as
# test/adrift5-games/): drop or symlink them into one of the GAMES dirs below,
# under the basename named in the MAP.  A row whose game is absent is SKIPped,
# a row whose solution is absent is NOSCRIPT -- neither fails the run.
#
# Env:
#   GAMES_DIR   primary game dir (default: harness/../games)
#   SCARE_DIR   engine sources for (re)building `scare` (default: terps/scarier)
# Determinism: the `scare` binary links seed.cpp (fixed RNG), so a given
# (game, solution) always yields the same transcript.
set -u
export LC_ALL=C

HERE="$(cd "$(dirname "$0")" && pwd)"
SCARE_BIN="$HERE/scare"
GAMES_DIR="${GAMES_DIR:-$HERE/../games}"
# Extra dirs searched (by basename) when a game isn't in GAMES_DIR.
ALT_DIRS="$HOME/adrift-battle/games"

BLESS=0; VERBOSE=0
case "${1:-}" in
  --bless) BLESS=1; shift ;;
  -v)      VERBOSE=1; shift ;;
esac
FILTER="${1:-}"

# solution file | game .taf basename | optional win marker (grep -F; "" = none)
#              | optional env assignments (space-separated VAR=val, applied to
#                the scare run -- e.g. SCR_SEED=2, SCR_ASSUME_COMBAT=1)
#
# Seeded with the two 4th-1-Hour-Comp games already carried here, plus the
# ready-to-add native-ADRIFT Plover games (they SKIP until their .taf is
# dropped into a games dir and a *_solution.txt is derived -- see
# TODO_plover_walkthroughs.md §1/§6).  Add a row per game as you derive it.
#
# (A function-wrapped heredoc, NOT MAP=$(cat <<EOF): macOS /bin/bash 3.2
# mis-parses heredocs inside $() when the content's quote count is odd --
# an apostrophe in a marker would break the whole script.)
map_rows() { cat <<'EOF'
icecream_solution.txt|IceCream.taf|
the_cat_in_the_tree_solution.txt|TheCatintheTree.taf|Congratulations!
man_overboard_solution.txt|man overboard.taf|Maybe it wasn't all a waste of time
pieces_of_eden_solution.txt|Pieces of eden.taf|END OF PART ONE
princess_in_the_tower_solution.txt|princess1.taf|It seems you've won.
too_much_exercise_solution.txt|exercise.taf|Congratulations!
yak_shaving_solution.txt|yak_shaving.taf|completed the Odd Competition
buried_alive_solution.txt|buried.taf|Well done. You got to the end
confession_solution.txt|Confession(1).taf|Striking a plea deal
snakes_and_ladders_solution.txt|sandl.taf|made it to the end of the game
veteran_solution.txt|veteran.taf|fulfilling your destiny
togetyou_solution.txt|togetyou.taf|another flesh-sack
zombies_solution.txt|ZAC.taf|you and Stu were eaten by zombies
adrift_maze_solution.txt|ADRIFTMaze.taf|You WIN!
cruel_solution.txt|CAH.taf|destroyed our reality
trabula_solution.txt|Trabula.taf|given the gold coins to Trabula
shred_em_solution.txt|shreddem.taf|Due to lack of evidence
shadowpeak_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak
shadowpeak_allgargoyles_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak
shadowpeak_killwraith_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak
alexis_solution.txt|ALEXIS.TAF|you have beaten Urgorn
alexis_worn_cube_solution.txt|ALEXIS.TAF|you have beaten Urgorn
topaz_solution.txt|topaz.taf|The two of you set out into the forest.
thorn_solution.txt|Thorn.taf|You have chosen to look upon your own mortality.
renegade_brainwave_solution.txt|Renegade_Brainwave.taf|planet Earth has been averted!
goldilocks_solution.txt|goldilocks.taf|Three Bears are no more
masochists_heaven_solution.txt|1HRGAME.taf|Congratulations!
griswold_solution.txt|Griswold.taf|And there you have it: the intro
mhpquest_solution.txt|mhpquest.taf|You have saved Crystal's life
# Archie's Birthday is AIF: the game's text is sexually explicit, so its solution
# and golden are deliberately NOT committed (they are in harness/.gitignore).  The
# row stays so the regression runs where the files exist; elsewhere it NOSCRIPTs.
archie_solution.txt|Archie's Birthday V 1-2.taf|To be continued
# The adrift-battle corpus (the WALKTHROUGH_TODO.md games, banked 2026-06) --
# wins first, then documented-max tours / sandboxes / demos.  Tour rows use the
# final "Your score is N out of a maximum of M." line as their marker so the
# documented maxima stay locked; win rows use the game's own victory text.
bomb_threat_solution.txt|Bomb Threat.taf|Congratulations!
circus_solution.txt|circus.taf|Congratulations.  You completed the game|SCR_SEED=2
colony_solution.txt|Colony.taf|Congratulations!
cyber_solution.txt|cyber.taf|THE END,or is it?
cyber2_solution.txt|cyber2.taf|you have beaton Cyber Warp 2!
cybercow_win_solution.txt|lair-of-the-cybercow.taf|Thank you for playing Lair of the CyberCow.
cybercow_solution.txt|lair-of-the-cybercow.taf|Your score is 6 out of a maximum of 10.
deaths_solution.txt|deaths.taf|crumbles into dust
donuts_intro_solution.txt|donuts_intro.taf|To be continued (maybe)..
funhouse_solution.txt|FunHouse.taf|thank you for bravely protecting this important information
gateway_solution.txt|gateway.taf|THE END
hyper_b_s_solution.txt|hyper_b_s.taf|The Flare Rat is dead! Mission complete!
jason_vs_salm_solution.txt|Jason Vs. Salm.taf|Good job then!
light_up_solution.txt|light_up_4summer_comp.taf|THE END
maincourse_solution.txt|Main Course.taf|You're on your way home with just a little indigestion!
melbourne_beach_solution.txt|Melbourne Beach.taf|You successfully completed the original game Melbourne Beach
orient_express_solution.txt|Orient_Express.taf|You successfully complete your assignment.
screen_savers_solution.txt|The Screen Savers On Planet X.taf|You've managed to get everyone to the set!
secret_of_lost_world_solution.txt|SecretOfLostWorld.taf|The ship is slowly sailing away
space_boy_solution.txt|Space Boy's First Adventure.taf|STAY TUNED FOR MORE EXCITING EPISODES
sun_empire_solution.txt|Sun_Empire_Quest_For_The_Founders.taf|Congratulations!
tcom_solution.txt|tcom.taf|the file entitled "tcom2"
think2_solution.txt|Theannihilationofthink2.taf|Think.com has been restored
toxically_earth_solution.txt|Toxically_Earth.taf|Thanks for playing RON: TOXICALLY EARTH
xfiles_solution.txt|The_X-Files_A_New_Beginning.taf|Welcome to the Resistance.
del_sol_solution.txt|Del Sol.taf|Your score is 24 out of a maximum of 46.
inverness_solution.txt|inverness.taf|A murderer thou shalt be
les_feux_solution.txt|Les Feux de l'enfer.taf|Votre score est 25 sur un maximum de 115.
lifesimulation_solution.txt|lifesimulation.taf|Your score is 0 out of a maximum of 0.
matts_house_solution.txt|Matt's House.taf|Your score is 5 out of a maximum of 5.
mr_smith_solution.txt|The_Search_For_Mr_Smith.taf|Your score is 25 out of a maximum of 100.
phoenix_destiny_solution.txt|Phoenix_Destiny.taf|Gold: 100
questi_solution.txt|QuestI.taf|Your score is 5 out of a maximum of 10.
shadow_of_the_past_solution.txt|Shadow_Of_The_Past.taf|You now realize that the statue was you from a past life.
spirits_flight_solution.txt|The_Spirits_Flight.taf|Your score is 50 out of a maximum of 95.
srsintro_solution.txt|SRSintro.taf|
the_nonsense_machine_6000_solution.txt|The_Nonsense_Machine_6000.taf|
the_town_of_azra_solution.txt|The_Town_Of_Azra.taf|Number of turns passed: 27
thetest_solution.txt|thetest.taf|Your score is 5 out of a maximum of 25.
through_time_solution.txt|Through time.taf|This is as far as this adventure will take you at this point.
to_hell_and_beyond_solution.txt|To_Hell_And_Beyond.taf|You have entered the town of Oran.
villains_and_kings_solution.txt|Villains_And_Kings.taf|Your score is 13 out of a maximum of 37.
villains_and_kings_assisted_solution.txt|Villains_And_Kings.taf|Your score is 30 out of a maximum of 37.|SCR_ASSUME_COMBAT=1
wes_ghn_solution.txt|WesGHN.taf|Your score is 30 out of a maximum of 100.
EOF
}

find_game() {  # $1=basename -> prints path or nothing
  if [ -f "$GAMES_DIR/$1" ]; then printf '%s\n' "$GAMES_DIR/$1"; return; fi
  for d in $ALT_DIRS; do
    [ -f "$d/$1" ] && { printf '%s\n' "$d/$1"; return; }
  done
}

# Run the seeded interpreter over a solution and normalise the transcript the
# same way the a5 golden path does (strip trailing ws, squeeze blank runs).
# ROW_ENV carries the row's optional env assignments (4th MAP field).
transcript() {  # $1=game path $2=solution path
  { cat "$2"; echo quit; echo y; } \
    | ( ulimit -t 30; env $ROW_ENV "$SCARE_BIN" "$1" 2>/dev/null ) \
    | tr -d '\r' | sed 's/[[:space:]]*$//' | cat -s
}

# Build the harness if it's missing (or stale isn't tracked -- rebuild is cheap
# enough only when absent; run build.sh by hand to force a rebuild).
if [ ! -x "$SCARE_BIN" ]; then
  echo "building headless scare harness (build.sh)..." >&2
  SCARE_DIR="${SCARE_DIR:-}" sh "$HERE/build.sh" >&2 || {
    echo "run_v4_walkthroughs: build failed" >&2; exit 2; }
fi

REGFILE=$(mktemp); trap 'rm -f "$REGFILE"' EXIT
printf "%-34s %-9s %s\n" "SOLUTION" "STATUS" "detail"
printf "%-34s %-9s %s\n" "--------" "------" "------"

map_rows | while IFS='|' read -r sol game marker envs; do
  [ -z "$sol" ] && continue
  case "$sol" in '#'*) continue ;; esac       # comment row
  case "$sol" in *"$FILTER"*) : ;; *) continue ;; esac
  ROW_ENV=$envs
  solpath="$HERE/$sol"
  golden="$HERE/${sol%.txt}.expected.txt"

  [ -f "$solpath" ] || { printf "%-34s %-9s\n" "$sol" "NOSCRIPT"; continue; }
  gp=$(find_game "$game")
  [ -n "$gp" ] || { printf "%-34s %-9s (%s)\n" "$sol" "SKIP" "$game"; continue; }

  out=$(transcript "$gp" "$solpath")

  # Optional win-marker guard.
  markok=1
  if [ -n "$marker" ]; then
    printf '%s\n' "$out" | grep -Fq "$marker" || markok=0
  fi

  if [ "$BLESS" = 1 ]; then
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (win marker '%s' absent -- NOT blessed)\n" "$sol" "REFUSED" "$marker"
      echo "$sol" >> "$REGFILE"
    else
      printf '%s\n' "$out" > "$golden"
      printf "%-34s %-9s -> %s\n" "$sol" "BLESSED" "$(basename "$golden")"
    fi
    continue
  fi

  if [ ! -f "$golden" ]; then
    # No golden yet: not a hard failure, but flag it, and fail if a declared
    # win marker is missing (a losing transcript must never look "ok").
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (no golden AND win marker '%s' absent)\n" "$sol" "FAIL" "$marker"
      echo "$sol" >> "$REGFILE"
    else
      printf "%-34s %-9s (run --bless to record)\n" "$sol" "NEEDGOLD"
    fi
    continue
  fi

  if printf '%s\n' "$out" | diff -q "$golden" - >/dev/null 2>&1 && [ "$markok" = 1 ]; then
    printf "%-34s %-9s\n" "$sol" "PASS"
  else
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (win marker '%s' absent)\n" "$sol" "FAIL" "$marker"
    else
      printf "%-34s %-9s (golden mismatch)\n" "$sol" "FAIL"
    fi
    [ "$VERBOSE" = 1 ] && printf '%s\n' "$out" | diff "$golden" - | sed 's/^/    /'
    echo "$sol" >> "$REGFILE"
  fi
done

echo
echo "PASS = transcript matches golden (+ win marker if set); NEEDGOLD = derived"
echo "but not yet recorded (run --bless); SKIP = game .taf absent; NOSCRIPT = no"
echo "solution file; FAIL = golden mismatch or missing win marker."

if [ -s "$REGFILE" ]; then
  echo; echo "REGRESSIONS: $(tr '\n' ' ' < "$REGFILE")"
  exit 1
fi
exit 0
