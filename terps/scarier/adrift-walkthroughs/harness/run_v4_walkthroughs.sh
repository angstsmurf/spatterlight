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
# Extra dirs searched (by basename) when a game isn't in GAMES_DIR. The whole
# corpus now lives in GAMES_DIR (the ~/adrift-battle/games working mirror was
# folded into it on 2026-07-22), so this is only a hook for a machine that keeps
# its .taf files somewhere else -- set it in the environment there.
ALT_DIRS="${ALT_DIRS:-}"

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
snakes_and_ladders_solution.txt|sandl.taf|made it to the end of the game|SCR_SEED=2
veteran_solution.txt|veteran.taf|fulfilling your destiny
togetyou_solution.txt|togetyou.taf|another flesh-sack
zombies_solution.txt|ZAC.taf|you and Stu were eaten by zombies
adrift_maze_solution.txt|ADRIFTMaze.taf|You WIN!
cruel_solution.txt|CAH.taf|destroyed our reality
trabula_solution.txt|Trabula.taf|given the gold coins to Trabula
shred_em_solution.txt|shreddem.taf|Due to lack of evidence
shadowpeak_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=1
shadowpeak_allgargoyles_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=20
shadowpeak_killwraith_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=155
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
circus_solution.txt|circus.taf|Congratulations.  You completed the game|SCR_SEED=17
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
jason_vs_salm_solution.txt|Jason Vs. Salm.taf|Good job then!|SCR_SEED=11
light_up_solution.txt|light_up_4summer_comp.taf|THE END|SCR_SEED=45
maincourse_solution.txt|Main Course.taf|You're on your way home with just a little indigestion!|SCR_SEED=17
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
del_sol_solution.txt|Del Sol.taf|Your score is 26 out of a maximum of 46.
inverness_solution.txt|inverness.taf|A murderer thou shalt be
les_feux_solution.txt|Les Feux de l'enfer.taf|Votre score est 25 sur un maximum de 115.|SCR_SEED=138
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
# thetest IS winnable (2026-08-01, verified live in run390 to "Well done!  You
# won!" at 20/25): the colour-door needs addything==3, i.e. two consecutive
# key/door colour matches on `unlock door` (the old "circular lock /
# unwinnable" verdict misindexed task 15's variable restriction -- var1-2 is
# addything, not robot2).  The route's unlock/shout spam is RNG-timing under
# the fixed seed; see thetest_walkthrough.md for the mechanism.
thetest_win_solution.txt|thetest.taf|Well done!  You won!
through_time_solution.txt|Through time.taf|This is as far as this adventure will take you at this point.
to_hell_and_beyond_solution.txt|To_Hell_And_Beyond.taf|You have entered the town of Oran.
# The assisted To-Hell row needs BOTH aids: the game's combat data is all-zero
# accuracy/agility AND its mid-game progression moves have an unset "To:" combo
# (Var2=-1).  With only SCR_ASSUME_COMBAT the player never leaves the mansion
# and the closing "claim the throne" is not understood (the 2026-07-14 "desync"
# was exactly that -- a replay missing SCR_ASSUME_MOVES).
to_hell_and_beyond_assisted_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1
villains_and_kings_solution.txt|Villains_And_Kings.taf|Your score is 13 out of a maximum of 37.
villains_and_kings_assisted_solution.txt|Villains_And_Kings.taf|Your score is 30 out of a maximum of 37.|SCR_ASSUME_COMBAT=1
# WesGHN's old "UNWINNABLE 30/100, orphaned gold ring" verdict was wrong
# (2026-08-02): event 1 [Davidshand] -- started by `ring bell`, misread in the
# original dump because EVENT o2/o3 print RAW 1-based refs -- drops the severed
# hand (ring attached) into the Waiting Room one turn later.  Full win, all 12
# scoring tasks, Hope killed twice (#Hopedies + #Hopedies2).
wes_ghn_solution.txt|WesGHN.taf|You've Won the Game!
# The eleven .taf files that were sitting in games/ unwired (2026-08-02).  Nine
# are winnable and use the game's own victory text as the marker; Invasion of
# the Second-Hand Shirts has no EndGame action anywhere in its task table, so
# its row is a tour to the last room.  (Woof prints "My score", not "Your
# score" -- the corpus's usual tour marker would not have matched.)
argh_solution.txt|ARGH_sGreatEscape.taf|Congratulations!
spam_solution.txt|SPAM.taf|Spam King
wreckage_solution.txt|Wreckage.taf|you've rescued yourself
vagabond_solution.txt|Vagabond.taf|The End
woof_solution.txt|Woof.taf|I'm back.
undefined_solution.txt|Undefined1.taf|An end is defined.
ecod3_solution.txt|ECOD3.taf|Congratulations!
goblinhunt_solution.txt|goblinhunt.taf|Congratulations!
agent4f_solution.txt|agent_4F[1].A.taf|Congratulations!
invasion_shirts_solution.txt|Invasion of the Second-Hand Shirts.taf|You're floating through the air above the trees.
adriftorama_solution.txt|adriftorama.taf|*****You Win!*****
# The seventeen games swept out of the Key & Compass ADRIFT index (2026-08-02);
# see WALKTHROUGH_TODO.md "2026-08-02 (later)" for where each .taf came from.
wax_worx_solution.txt|wax_worx.taf|[PRESS ANY KEY TO DIE]
sommeril_solution.txt|sommeril.taf|www.angelfire.com/games5/sommeril
dragonshrine_solution.txt|DragonShrineR43.taf|ended the Curse of Dragon Shrine
shardsofmemory_solution.txt|shardsofmemory.taf|My adventure has ended, and in victory besides
TheADRIFTProject_solution.txt|TheADRIFTProject.taf|the entire ADRIFT community greet you
ShadricksUnderground_solution.txt|ShadricksUnderground.taf|the robbers were caught red handed in the vault
ticket_solution.txt|ticket.taf|You won and managed to score 110 out of a possible 110
cleft_solution.txt|cleft.taf|You have successfully completed the Cleft in the Rock
Tear_solution.txt|Tear.taf|Suddenly the world seems a brighter place, and you feel there is a good
tq3_solution.txt|tq3.taf|you have sucessfully completed my first IF game
yeh_solution.txt|yeh.taf|Your score is 3100 out of a maximum of 3400.
ADRIFTMAS_Party_solution.txt|ADRIFTMAS_Party.taf|"Merry ADRIFTMAS TO ALL!  And to all a good night!"
Glum_Fiddle_solution.txt|Glum Fiddle.taf|Your score:100 out of 100.
JGrim_solution.txt|JGrim1.0.taf|WHOOOOOSH
mysteryofcaves_solution.txt|mysteryofcaves.taf|Your finishing rank is: Godlike Adventurer.
chooseyourown_solution.txt|chooseyourown.taf|"A hunch," you say. You link arms with Sharon Elson.
fantasyworld_solution.txt|fantasyworld.taf|Congratulations!
sophie_solution.txt|sa.taf|You have won.|SCR_SKIP_WAITKEY=1
sophie_comp_solution.txt|sophie.taf|You have won.|SCR_SKIP_WAITKEY=1
# The twenty-one entries of the 1st, 2nd and 3rd ADRIFT One-Hour Game
# Competitions (2003), swept in on 2026-08-03 -- see WALKTHROUGH_TODO.md
# "2026-08-03" for where each .taf came from and for the per-game notes.
# Several of these are deliberately unwinnable or end in the player's death;
# the marker is the game's own final line in each case, not a victory string.
# 1st One-Hour Game Competition
frog_solution.txt|frog.taf|So you hop away with your fairy princess, to live hoppily ever after.
chicken_solution.txt|chicken.taf|That was the last time either of you threw a brick at something.
endgame_solution.txt|endgame.taf|Really really.
hauntedhouse_solution.txt|hauntedhouse.taf|you congraulate yourself on a job well done.
microbe_willie_solution.txt|microbe_willie.taf|pestilence (basically, more of your kind) throughout the world.
amonkeytoomany_solution.txt|amonkeytoomany.taf|Hooray! You've made it through the game!
# 2nd One-Hour Game Competition
dfu_solution.txt|DFU.taf|Thank you, and good night.
percy_solution.txt|Percy.taf|prince among vikings
forum_solution.txt|forum.taf|You Won!
# 3rd One-Hour Game Competition
cbn_solution.txt|CBN.taf|you excelled yourself
cbn2_solution.txt|cbn2.taf|the archives room goes up in flames
crm_solution.txt|CRM.taf|You take a long bow as the curtains close for the show, and the dead body
ecod2_solution.txt|ECOD2.taf|has been captured
imagination_solution.txt|Imagination.taf|Was this all just in your imagination?
asdfa_solution.txt|asdfa.taf|bottle of Nightmare Inducer fluid back in his pocket
demonhunter_solution.txt|demonhunter.taf|journey to the beginning of your new life. You're a demonhunter.
forum2_solution.txt|forum2.taf|***You have won!***
pyramid_solution.txt|pyramid.taf|moves out of your way allowing you to make a hasty retreat.
saffire_solution.txt|saffire.taf|you reach heaven
shore_solution.txt|shore.taf|an island shrouded in a steel fog.
ticktick_solution.txt|ticktick.taf|I'm afraid you are dead!
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

# Build the harness if it's missing OR older than any engine source.  The
# missing-only check once let a whole corpus run "pass" against a stale binary
# (the wield-model port, 2026-08-01) -- never again.
SRC_DIR="${SCARE_DIR:-$(cd "$HERE/../.." && pwd)}"
if [ ! -x "$SCARE_BIN" ] \
   || [ -n "$(find "$SRC_DIR" -maxdepth 1 \( -name 'sc*.cpp' -o -name '*.h' \) \
              -newer "$SCARE_BIN" 2>/dev/null | head -1)" ]; then
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
