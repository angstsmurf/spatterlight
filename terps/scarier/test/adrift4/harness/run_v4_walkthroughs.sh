#!/bin/sh
# Deterministic ADRIFT-3.9/4.0 (SCARE-engine) walkthrough regression, modelled
# on test/adrift5/harness/run_a5_walkthroughs.sh (which does the same job for the ADRIFT-5 a5
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
#   sh run_v4_walkthroughs.sh -j 4 [substring]    # cap the parallelism
#
# A solution's golden is  goldens/<solution-basename-sans-.txt>.expected.txt.
# Game .taf files are third-party data and are NOT committed (same policy as
# test/adrift5/games/): drop or symlink them into one of the GAMES dirs below,
# under the basename named in the MAP.  A row whose game is absent is SKIPped,
# a row whose solution is absent is NOSCRIPT -- neither fails the run.
#
# Env:
#   GAMES_DIR   primary game dir (default: harness/../games)
#   SCARE_DIR   engine sources for (re)building `scare` (default: terps/scarier)
#   V4WT_JOBS   rows to run at once (default: core count; -j overrides)
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
# Rows are independent (one `scare` process each, no shared state but the
# goldens they read), so they fan out across cores -- see the run loop at the
# foot of this file.
JOBS="${V4WT_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
while :; do
  case "${1:-}" in
    --bless) BLESS=1; shift ;;
    -v)      VERBOSE=1; shift ;;
    -j)      JOBS="$2"; shift 2 ;;
    *)       break ;;
  esac
done
[ "$JOBS" -ge 1 ] 2>/dev/null || JOBS=1
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
# Each row's measurement evidence and re-bless history lives in
# ../notes/v4_walkthrough_rows.md, under the row's solution file; only
# notes of three lines or fewer stay inline above their row.  Put new
# notes there, not here.
#
# (A function-wrapped heredoc, NOT MAP=$(cat <<EOF): macOS /bin/bash 3.2
# mis-parses heredocs inside $() when the content's quote count is odd --
# an apostrophe in a marker would break the whole script.)
map_rows() { cat <<'EOF'
# 2026-09-27 re-blessed, lenient task matching (deliberate deviation): the original `put ice cream in cone` reaches its task again (the 4.0 put parser no longer clobbers the line to a fragment).
icecream_solution.txt|IceCream.taf||SCR_SKIP_WAITKEY=1
the_cat_in_the_tree_solution.txt|TheCatintheTree.taf|You scored 50 out of the maximum 50!|SCR_SKIP_WAITKEY=1
man_overboard_solution.txt|man overboard.taf|Maybe it wasn't all a waste of time|SCR_SKIP_WAITKEY=1
pieces_of_eden_solution.txt|Pieces of eden.taf|END OF PART ONE
# Measured 2026-08-29 in run400 under Wine: 78/78 commands identical (Adrift_1_princess.txt).
princess_in_the_tower_solution.txt|princess1.taf|It seems you've won.
# Measured 2026-08-29: run400 replay, all 48 commands echoed, 48 of 48 turns
# identical bar the [Press any key to end] tail on the last.
too_much_exercise_solution.txt|exercise.taf|much prefer that Sweet Shop option one of your work colleagues took.
yak_shaving_solution.txt|yak_shaving.taf|completed the Odd Competition|SCR_SKIP_WAITKEY=1
buried_alive_solution.txt|buried.taf|Well done. You got to the end
confession_solution.txt|Confession(1).taf|Striking a plea deal|SCR_SKIP_WAITKEY=1
snakes_and_ladders_solution.txt|sandl.taf|made it to the end of the game|SCR_SEED=2
veteran_solution.txt|veteran.taf|fulfilling your destiny
togetyou_solution.txt|togetyou.taf|another flesh-sack|SCR_SKIP_WAITKEY=1
zombies_solution.txt|ZAC.taf|you and Stu were eaten by zombies|SCR_SEED=2 SCR_SKIP_WAITKEY=1
adrift_maze_solution.txt|ADRIFTMaze.taf|You WIN!
cruel_solution.txt|CAH.taf|destroyed our reality
trabula_solution.txt|Trabula.taf|given the gold coins to Trabula
shred_em_solution.txt|shreddem.taf|Due to lack of evidence
shadowpeak_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=1
shadowpeak_allgargoyles_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=1
shadowpeak_killwraith_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=1
alexis_solution.txt|ALEXIS.TAF|you have beaten Urgorn|SCR_SEED=1
alexis_worn_cube_solution.txt|ALEXIS.TAF|you have beaten Urgorn|SCR_SEED=2
topaz_solution.txt|topaz.taf|The two of you set out into the forest.|SCR_SKIP_WAITKEY=1
thorn_solution.txt|Thorn.taf|You have chosen to look upon your own mortality.
renegade_brainwave_solution.txt|Renegade_Brainwave.taf|planet Earth has been averted!
goldilocks_solution.txt|goldilocks.taf|Three Bears are no more
# Measured 2026-09-05: full run400 replay under Wine (Adrift_43_1hrgame.txt,
# feed cmdfile_w_masochists_heaven.txt, 13 commands, PRE=1).  13/13 echoed,
# tail only; 15/15 both sides.
masochists_heaven_solution.txt|1HRGAME.taf|You scored 15 out of the maximum 15!
griswold_solution.txt|Griswold.taf|And there you have it: the intro|SCR_SKIP_WAITKEY=1
mhpquest_solution.txt|mhpquest.taf|You have saved Crystal's life
archie_solution.txt|Archie's Birthday V 1-2.taf|To be continued|SCR_SKIP_WAITKEY=1
bomb_threat_solution.txt|Bomb Threat.taf|Or have you...|SCR_SEED=9
circus_sold_points_solution.txt|circus.taf|Congratulations.  You completed the game|SCR_SEED=12 SCR_SKIP_WAITKEY=1
circus_solution.txt|circus.taf|You scored 140 out of the maximum 140!|SCR_SEED=2 SCR_SKIP_WAITKEY=1
colony_solution.txt|Colony.taf|You scored 200 out of the maximum 200!
cyber_solution.txt|cyber.taf|THE END,or is it?
cyber2_solution.txt|cyber2.taf|you have beaton Cyber Warp 2!
cybercow_win_solution.txt|lair-of-the-cybercow.taf|Thank you for playing Lair of the CyberCow.
cybercow_solution.txt|lair-of-the-cybercow.taf|Your score is 6 out of a maximum of 10.
deaths_solution.txt|deaths.taf|crumbles into dust
# Re-blessed 2026-09-07 for the same capitalisation rule as the trabula row above: `wife hits you with the pot.` -> `Wife`.  This row's transcript (Adrift_176) never reaches the battle turn, so the line follows from the rule rather than from a measurement of its own.
donuts_intro_solution.txt|donuts_intro.taf|To be continued (maybe)..
funhouse_solution.txt|FunHouse.taf|thank you for bravely protecting this important information
funhouse_patched_solution.txt|FunHouse.taf|You scored 410 out of the maximum 410!|SCR_ASSUME_PATCHES=1
gateway_solution.txt|gateway.taf|THE END
hyper_b_s_solution.txt|hyper_b_s.taf|The Flare Rat is dead! Mission complete!
jason_vs_salm_solution.txt|Jason Vs. Salm.taf|Good job then!|SCR_SEED=3
light_up_solution.txt|light_up_4summer_comp.taf|THE END|SCR_SEED=133
maincourse_solution.txt|Main Course.taf|Congratulations! You're on your way home with just a little indigestion!|SCR_SEED=17
melbourne_beach_solution.txt|Melbourne Beach.taf|You successfully completed the original game Melbourne Beach
melbourne_patched_solution.txt|Melbourne Beach.taf|You scored 39 out of the maximum 41!|SCR_ASSUME_PATCHES=1
orient_express_solution.txt|Orient_Express.taf|You successfully complete your assignment.
screen_savers_solution.txt|The Screen Savers On Planet X.taf|You've managed to get everyone to the set!
secret_of_lost_world_solution.txt|SecretOfLostWorld.taf|The ship is slowly sailing away
space_boy_solution.txt|Space Boy's First Adventure.taf|STAY TUNED FOR MORE EXCITING EPISODES
space_boy_patched_solution.txt|Space Boy's First Adventure.taf|STAY TUNED FOR MORE EXCITING EPISODES|SCR_ASSUME_PATCHES=1
sun_empire_solution.txt|Sun_Empire_Quest_For_The_Founders.taf|You scored 140 out of the maximum 145!|SCR_SEED=10
sun_empire_patched_solution.txt|Sun_Empire_Quest_For_The_Founders.taf|You scored 145 out of the maximum 145!|SCR_SEED=10 SCR_ASSUME_PATCHES=1
tcom_solution.txt|tcom.taf|the file entitled "tcom2"
think2_solution.txt|Theannihilationofthink2.taf|Think.com has been restored
toxically_earth_solution.txt|Toxically_Earth.taf|Thanks for playing RON: TOXICALLY EARTH
xfiles_solution.txt|The_X-Files_A_New_Beginning.taf|Welcome to the Resistance.
del_sol_solution.txt|Del Sol.taf|Your score is 26 out of a maximum of 46.
del_sol_patched_solution.txt|Del Sol.taf|You scored 45 out of the maximum 46!|SCR_ASSUME_PATCHES=1
inverness_solution.txt|inverness.taf|You hear Macbeth and his wife leave the room.|SCR_SEED=2
les_feux_solution.txt|Les Feux de l'enfer.taf|Votre score est 75 sur un maximum de 115.|SCR_SEED=45 SCR_SKIP_WAITKEY=1
lifesimulation_solution.txt|lifesimulation.taf|Your score is 0 out of a maximum of 0.
matts_house_solution.txt|Matt's House.taf|Your score is 5 out of a maximum of 5.
mr_smith_solution.txt|The_Search_For_Mr_Smith.taf|You scored 90 out of the maximum 100!
phoenix_destiny_solution.txt|Phoenix_Destiny.taf|Gold: 100
questi_solution.txt|QuestI.taf|Your score is 10 out of a maximum of 10.
shadow_of_the_past_solution.txt|Shadow_Of_The_Past.taf|You now realize that the statue was you from a past life.
spirits_flight_solution.txt|The_Spirits_Flight.taf|Your score is 50 out of a maximum of 95.
spirits_flight_patched_solution.txt|The_Spirits_Flight.taf|You scored 95 out of the maximum 95!|SCR_ASSUME_PATCHES=1
srsintro_solution.txt|SRSintro.taf|
the_nonsense_machine_6000_solution.txt|The_Nonsense_Machine_6000.taf|
the_town_of_azra_solution.txt|The_Town_Of_Azra.taf|Number of turns passed: 26
the_town_of_azra_v390_solution.txt|The Town Of Azra.taf|Number of turns passed: 62|SCR_SKIP_WAITKEY=1
thetest_solution.txt|thetest.taf|Your score is 5 out of a maximum of 25.|SCR_SKIP_WAITKEY=1
thetest_win_solution.txt|thetest.taf|Well done!  You won!|SCR_SKIP_WAITKEY=1 SCR_SEED=8
through_time_solution.txt|Through time.taf|This is as far as this adventure will take you at this point.
to_hell_and_beyond_solution.txt|To_Hell_And_Beyond.taf|You have entered the town of Oran.
to_hell_and_beyond_assisted_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1 SCR_SKIP_WAITKEY=1
to_hell_and_beyond_assisted_max_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1 SCR_SKIP_WAITKEY=1
villains_and_kings_solution.txt|Villains_And_Kings.taf|Your score is 30 out of a maximum of 37.
villains_and_kings_patched_solution.txt|Villains_And_Kings.taf|Your score is 31 out of a maximum of 37.|SCR_ASSUME_PATCHES=1
wes_ghn_solution.txt|WesGHN.taf|You've Won the Game!|SCR_SEED=2
argh_solution.txt|ARGH_sGreatEscape.taf|You scored 100 out of the maximum 125!
spam_solution.txt|SPAM.taf|Spam King|SCR_SKIP_WAITKEY=1
# Measured 2026-09-05: full run400 replay under Wine (Adrift_47_wreckage.txt,
# feed cmdfile_w_wreckage.txt, 11 commands, PRE=0).  11/11 echoed, tail only;
# the winning `use the computer` matches to the last word.
wreckage_solution.txt|Wreckage.taf|you've rescued yourself
vagabond_solution.txt|Vagabond.taf|The End|SCR_SKIP_WAITKEY=1
# Re-blessed 2026-09-19: an empty authored PlayerName is "Anonymous" at 4.0 (probe ANON, Adrift_1198), not "Player".
woof_solution.txt|Woof.taf|I'm back.|SCR_SEED=5
undefined_solution.txt|Undefined1.taf|An end is defined.
ecod3_solution.txt|ECOD3.taf|In an alley behind Denny's.
goblinhunt_solution.txt|goblinhunt.taf|Tomorrow is the next goblin hunt.|SCR_SKIP_WAITKEY=1
agent4f_solution.txt|agent_4F[1].A.taf|You wake with a start.  What a terrible dream!
# Measured 2026-08-29: run400 replay, 13 commands echoed and all 13 identical
# before a real-time <wait> pause ate the next one; a cmdfile with #sleep lines
# for those pauses is the follow-up.
invasion_shirts_solution.txt|Invasion of the Second-Hand Shirts.taf|You're floating through the air above the trees.
adriftorama_solution.txt|adriftorama.taf|*****You Win!*****|SCR_SEED=18 SCR_SKIP_WAITKEY=1
# The seventeen games swept out of the Key & Compass ADRIFT index (2026-08-02);
# see the per-game notes/*_walkthrough.md for where each .taf came from.
wax_worx_solution.txt|wax_worx.taf|[PRESS ANY KEY TO DIE]
sommeril_solution.txt|sommeril.taf|www.angelfire.com/games5/sommeril
sommeril_patched_solution.txt|sommeril.taf|You scored 95 out of the maximum 100!|SCR_ASSUME_PATCHES=1
dragonshrine_solution.txt|DragonShrineR43.taf|ended the Curse of Dragon Shrine|SCR_SKIP_WAITKEY=1
shardsofmemory_solution.txt|shardsofmemory.taf|My adventure has ended, and in victory besides|SCR_SKIP_WAITKEY=1
TheADRIFTProject_solution.txt|TheADRIFTProject.taf|the entire ADRIFT community greet you|SCR_SKIP_WAITKEY=1
ShadricksUnderground_solution.txt|ShadricksUnderground.taf|the robbers were caught red handed in the vault|SCR_SKIP_WAITKEY=1
ticket_solution.txt|ticket.taf|You won and managed to score 110 out of a possible 110|SCR_SEED=10 SCR_SKIP_WAITKEY=1
cleft_solution.txt|cleft.taf|You scored 100 out of the maximum 100!
# Measured 2026-08-29 in run400 under Wine: 36/36 commands identical (Adrift_1_Tear.txt).
Tear_solution.txt|Tear.taf|Suddenly the world seems a brighter place, and you feel there is a good
tq3_solution.txt|tq3.taf|Please forward your comments to chris@jons.org.
yeh_solution.txt|yeh.taf|Your score is 3100 out of a maximum of 3400.
ADRIFTMAS_Party_solution.txt|ADRIFTMAS_Party.taf|"Merry ADRIFTMAS TO ALL!  And to all a good night!"|SCR_SKIP_WAITKEY=1
Glum_Fiddle_solution.txt|Glum Fiddle.taf|Your score:100 out of 100.|SCR_SKIP_WAITKEY=1
# 2026-09-27 re-blessed, lenient task matching (deliberate deviation): `in` works again despite TASK 91's trailing space.
JGrim_solution.txt|JGrim1.0.taf|WHOOOOOSH|SCR_SKIP_WAITKEY=1
# Measured 2026-09-07: re-driven in run400 with the corrected feed
# (Adrift_435_mysteryofcaves.txt, 115/115 echoed) -- IDENTICAL on every turn.
mysteryofcaves_solution.txt|mysteryofcaves.taf|Your finishing rank is: Godlike Adventurer.|SCR_SKIP_WAITKEY=1
chooseyourown_solution.txt|chooseyourown.taf|"A hunch," you say. You link arms with Sharon Elson.|SCR_SKIP_WAITKEY=1
fantasyworld_solution.txt|fantasyworld.taf|You scored 0 out of the maximum 500!
sophie_solution.txt|sa.taf|You have won.|SCR_SKIP_WAITKEY=1
sophie_comp_solution.txt|sophie.taf|You have won.|SCR_SKIP_WAITKEY=1
cursed_solution.txt|cursed.taf|You achieved a score of 95 out of a possible|SCR_SKIP_WAITKEY=1
easter_solution.txt|easter.taf|***You have won***|
yonastoundingcastle_solution.txt|yonastoundingcastle.taf|Incredible victory!|SCR_SEED=3 SCR_SKIP_WAITKEY=1
frog_solution.txt|frog.taf|So you hop away with your fairy princess, to live hoppily ever after.
chicken_solution.txt|chicken.taf|That was the last time either of you threw a brick at something.
endgame_solution.txt|endgame.taf|Really really.
hauntedhouse_solution.txt|hauntedhouse.taf|you congraulate yourself on a job well done.
microbe_willie_solution.txt|microbe_willie.taf|pestilence (basically, more of your kind) throughout the world.
# Full run390 replay 2026-09-05, Adrift_9_amonkey.txt (12 commands, PRE=0):
# 12/12 echoed, 11 of the 12 turns identical, `unlock door with key`
# differing only by `[Press any key to end]`.  25/25 both sides.
amonkeytoomany_solution.txt|amonkeytoomany.taf|Hooray! You've made it through the game!
# 2nd One-Hour Game Competition
# Full run390 replay 2026-09-05, Adrift_9_dfu.txt (21 commands, PRE=0):
# 21/21 echoed, tail only.  999999999/999999999 both sides.
dfu_solution.txt|DFU.taf|Thank you, and good night.
percy_solution.txt|Percy.taf|prince among vikings
forum_solution.txt|forum.taf|You Won!|SCR_SEED=1 SCR_SKIP_WAITKEY=1
# 3rd One-Hour Game Competition
cbn_solution.txt|CBN.taf|you excelled yourself|SCR_SKIP_WAITKEY=1
# Measured 2026-09-07: re-driven in run400 with the corrected feed
# (Adrift_431_cbn2.txt, 21/21 echoed) -- identical on every turn apart from
# the Runner's own [Press any key to end].
cbn2_solution.txt|cbn2.taf|the archives room goes up in flames|SCR_SKIP_WAITKEY=1
# Full run390 replay 2026-09-05, Adrift_9_crm.txt (21 commands + 2 blank
# Returns for the mid-game waitkeys, PRE=0): 21/21 echoed, tail only.
# 25/25 both sides.
crm_solution.txt|CRM.taf|You take a long bow as the curtains close for the show, and the dead body
# Full run390 replay 2026-09-05, Adrift_9_ecod2.txt (24 commands, PRE=3):
# 24/24 echoed, tail only.
ecod2_solution.txt|ECOD2.taf|has been captured|SCR_SKIP_WAITKEY=1
# Measured 2026-08-29: run400 replay (name prompt via POPUP_ANSWERS), all 14
# commands echoed, 12 of 14 identical: turn 0 is the echoed name, turn 13 the
# [Press any key to end] tail.
imagination_solution.txt|Imagination.taf|Was this all just in your imagination?
# Measured 2026-09-07: re-driven in run400 with the corrected feed
# (Adrift_436_asdfa.txt, 27/27 echoed) -- identical on every turn apart from
# the Runner's own [Press any key to end].
asdfa_solution.txt|asdfa.taf|bottle of Nightmare Inducer fluid back in his pocket|SCR_SKIP_WAITKEY=1
# Measured 2026-08-29: run400 replay, all 18 commands echoed, 17 of 18 turns
# identical, the last differs only by the [Press any key to end] tail.
demonhunter_solution.txt|demonhunter.taf|journey to the beginning of your new life. You're a demonhunter.
forum2_solution.txt|forum2.taf|***You have won!***|SCR_SKIP_WAITKEY=1
# Measured 2026-09-07: re-driven in run400 with the corrected feed
# (Adrift_443_pyramid.txt, 11/11 echoed) -- identical on every turn apart from
# the Runner's own [Press any key to end].
pyramid_solution.txt|pyramid.taf|allowing you to make a hasty retreat.|SCR_SKIP_WAITKEY=1
saffire_solution.txt|saffire.taf|you reach heaven
shore_solution.txt|shore.taf|an island shrouded in a steel fog.
ticktick_solution.txt|ticktick.taf|I'm afraid you are dead!
ptbad_solution.txt|ptbad.taf|You Win! Yay!
vague_solution.txt|vague.taf|Nothingness returns.|SCR_SKIP_WAITKEY=1
escape_to_new_york_solution.txt|EscapeToNewYork.taf|You managed to score 100 out of 100 and completed all of your objectives.|SCR_SEED=2 SCR_SKIP_WAITKEY=1
unauthorized_termination_solution.txt|unauthorized.taf|Assignment Status: You have been successful.|SCR_SKIP_WAITKEY=1
where_are_my_keys_solution.txt|WhereAreMyKeys.taf|You start the car and head home.|SCR_SEED=13 SCR_SKIP_WAITKEY=1
to_hell_in_a_hamper_solution.txt|Hamper.taf|reached the incredible altitude of 37,000 feet|SCR_SKIP_WAITKEY=1
lost_solution.txt|LOST.TAF|place your foot on the path leading up the crumbling cliff|SCR_SKIP_WAITKEY=1
lost_down_solution.txt|LOST.TAF|has shown you a doorway back to that brighter world.|SCR_SKIP_WAITKEY=1
marika_solution.txt|marika.taf|I plan to enjoy every second of it.|SCR_SKIP_WAITKEY=1
vendetta_solution.txt|Vendetta.taf|The End|SCR_SKIP_WAITKEY=1
unraveling_god_solution.txt|unravel.taf|smile as the river burns through your flesh.|SCR_SKIP_WAITKEY=1
unraveling_god_lou_solution.txt|unravel.taf|smile fades and you feel the beginnings of fear.|SCR_SKIP_WAITKEY=1
mishmash_solution.txt|mishmash.taf|You have lived up to your name and survived again!|SCR_SKIP_WAITKEY=1
the_hangover_solution.txt|hangover.taf|Your score is 5 out of a maximum of 7.
the_hangover_patched_solution.txt|hangover.taf|You scored 7 out of the maximum 7|SCR_ASSUME_PATCHES=1
troll_solution.txt|Troll.taf|clean by dinner time, I'll bust your head in!|SCR_SKIP_WAITKEY=1
spot_of_bother_solution.txt|A_Spot_of_Bother.taf|a grand total of 100 out of 100|SCR_SKIP_WAITKEY=1
beanstalk_solution.txt|Beanstalk.taf|*** You have won ***
black_sheeps_gold_solution.txt|BlackSheepsGold.taf|You've beaten Black Sheep's Gold!|SCR_SKIP_WAITKEY=1
doomed_xycanthus_solution.txt|xycanthus.taf|Well done - you scored maximum points!
# Measured 2026-09-05 in run400 under Wine (Adrift_70_dancingevenhim.txt, feed
# cmdfile_w_dancing_even_him.txt): clean, 17/17 echoed, tail only.
dancing_even_him_solution.txt|dancingevenhim.taf|it is an anagram of Vending Machine|SCR_SKIP_WAITKEY=1
the_demon_hunter_solution.txt|TheDemonHunter.taf|"Well done, my good and faithful|SCR_SKIP_WAITKEY=1
qui_a_tue_dana_solution.txt|QuiATueDana.taf|MERCI A TOI CHRISTOPHE SANS QUI CE JEU N'AURAIT JAMAIS VU LE JOUR!|SCR_SKIP_WAITKEY=1
enquete_a_hauts_risques_solution.txt|EnqueteAHautsRisques.taf|Votre score est de 59 sur un maximum de 59!
shadricks_travels_solution.txt|ShadricksTravels.taf|You scored 100 out of the maximum 100!
monsters_solution.txt|Monsters_r2.taf|You scored 40 out of the maximum 40!
the_amulet_solution.txt|TheAmulet.taf|Congratulations!
locked_door_solution.txt|Locked_door_with_water_trap.taf|See if I ever dive with you two again|SCR_SKIP_WAITKEY=1
marooned_solution.txt|marooned.taf|Congratulations, you are no longer Marooned!|SCR_SEED=3
wrecked_solution.txt|wrecked.taf|Hope you enjoyed playing Wrecked.|SCR_SEED=15
mortality_solution.txt|mortality.taf|one of the two good endings|SCR_SKIP_WAITKEY=1
largo_winch_solution.txt|largo-winch.taf|Votre score est de 97 sur un maximum de 97!
3monkeys_solution.txt|3monkeys.taf|Congratulations, you did it!|SCR_SEED=149|SCR_SKIP_WAITKEY=1
humbug_solution.txt|humbug.taf|Grandad would probably describe you as a winner.. or a cheat.|SCR_SKIP_WAITKEY=1
crime_adventure_solution.txt|Crime_Adventure.taf|Mrs Fenwick was in no danger at all, it was a friend
thesisters_solution.txt|TheSisters.taf|lifeless body of Trisha Seabourne.|SCR_SKIP_WAITKEY=1
thepkgirl_solution.txt|the_pk_girl.taf|Your Secret Letter is: E|SCR_SEED=24 SCR_SKIP_WAITKEY=1
second_chance_solution.txt|second chance.taf|congratulating me on a job well done.|SCR_SKIP_WAITKEY=1
private_eye_solution.txt|Private Eye.taf|You achieved a score of 4.|SCR_SKIP_WAITKEY=1
plague_solution.txt|The Plague - Redux.taf|spilling zombie blood once|SCR_SKIP_WAITKEY=1
iqsfot_solution.txt|iqsfot.taf|Thus one courageous space cadet saved the fish|SCR_SEED=391 SCR_SKIP_WAITKEY=1
mangiasaur_solution.txt|Mangiasaur.taf|You scored 63 out of the maximum 74!|SCR_SEED=1
afdfr_solution.txt|AFDFR.taf|Life is good for Death.|SCR_SKIP_WAITKEY=1
akron_solution.txt|akron.taf|you brave adventurer, saved yourself
cave_solution.txt|cave.taf|You scored 1000 out of the maximum 1000!
haunt_solution.txt|haunt.taf|You scored 84 out of the maximum 84!
twilight_solution.txt|twilight.taf|Your score is 485 out of a maximum of 500
twilight_patched_solution.txt|twilight.taf|Your score is 500 out of a maximum of 500|SCR_ASSUME_PATCHES=1
# Measured 2026-09-05 in run380 (`Adven_1_haunted.rtf`, 116 commands, the
# winning `open gate` last): 115/115 echoed, 0 differences.  Its two events
# are RNG-timed but have no room list, so nothing they do is visible.
haunted_house_solution.txt|haunted.taf|You scored 1000 out of the maximum 1000!
great_escape_solution.txt|great.taf|You scored 1480 out of the maximum 1860!|SCR_SEED=2
# Re-blessed 2026-09-04: pre-3.9 delayed events roll one RNG draw later (no
# startup event tick); the measurement is on the haunt row.
tom_ceader_solution.txt|secret.taf|you did good work escaping from the town
timmy_reid_solution.txt|tra.taf|Thanks for getting us back home!
# Measured 2026-09-05 in run380 (`Adven_1_duck.rtf`, 13 commands, the winning
# `jump` last): 12/12 echoed, 0 differences.
duck_mccloud_solution.txt|duck.taf|You jump from the plane just in time and you survive the huge
# Measured 2026-09-05 in run380 (`Adven_1_first.rtf`, the 18 commands plus a
# dummy `look` -- `read book` prints the ending without an EndGame, so the
# Save Transcript can follow it): 18/18 echoed, identical on every turn.
fistandantalus_solution.txt|first.taf|Congradulations you have won the game
james_bond_solution.txt|jb2000.taf|YOU COMPLEATED THE MISSION! YOU LANDED WELL
# Measured 2026-09-05 in run380 (`Adven_1_microwaveman.rtf`, 9 commands, the
# winning `shoot man` last): 8/8 echoed, 0 differences.
microwave_man_solution.txt|microwaveman.taf|You scored 100 out of the maximum 100!
life_of_mike_solution.txt|mikes.taf|Ypu ask her out
super_liam_solution.txt|superliam.taf|congradulation you have defeated x1
alices_restaurant_solution.txt|arlo.taf|recording an album that will be that hit record
# Measured 2026-09-05 in run370 (`Adven_1_castle.rtf`, 17 commands, the
# winning `take treasure chest` last): 16/16 echoed, 0 differences.
castle_quest_solution.txt|castle.taf|Thanks for playing!
deadman_solution.txt|The Dead Man.taf|ABORT SUCSESFUL|SCR_SKIP_WAITKEY=1
baroo_solution.txt|baroo.taf|You scored 16 out of the maximum 16!
lair_solution.txt|Lair of the Vampire.taf|the lord of the vampires, lies dead|SCR_SEED=4 SCR_SKIP_WAITKEY=1

fugitive_solution.txt|Fugitive.taf|This is the proof of innocence|SCR_SKIP_WAITKEY=1
fugitive_patched_solution.txt|Fugitive.taf|You scored 646 out of the maximum 666!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1

mammoth_solution.txt|MammothVacuum.taf|After many testing trials|SCR_SKIP_WAITKEY=1
headless_solution.txt|headless.taf|as a teenage headless experiment|SCR_SKIP_WAITKEY=1
redwire_solution.txt|Cut_the_Red_Wire.taf|a maximum possible of 1. Well done.|SCR_SKIP_WAITKEY=1
law_solution.txt|I am the Law.taf|out for Enterprise Research.|SCR_SKIP_WAITKEY=1
inmemory_solution.txt|InMemory.taf|had ceased to beep.|SCR_SKIP_WAITKEY=1
valley_solution.txt|valley.taf|and live happily ever after.|SCR_SKIP_WAITKEY=1
imagidroids_solution.txt|imagi.taf|You choose to put him out of his misery.|SCR_SKIP_WAITKEY=1
crimsondetritus_solution.txt|CD.taf|until the next victim comes along to take your place.|SCR_SKIP_WAITKEY=1
chosen_solution.txt|Chosen.taf|You plug the T-shaped block into the final socket in the door.|SCR_SKIP_WAITKEY=1
cellar_solution.txt|TheCellar.taf|And so The Cellar has ended. Many thanks for playing.|SCR_SKIP_WAITKEY=1
panic_solution.txt|panic.taf|Your rating is Messiah.|SCR_SEED=2 SCR_SKIP_WAITKEY=1
i_solution.txt|i.taf|I am dead.
dreamland_solution.txt|Dreams.taf|You have saved the Dreamworld
forest_on_the_norm_solution.txt|forest.taf|Thank you for playing my Aliengame
bob_bobsly_solution.txt|BobBobsly.taf|You scored 155 out of the maximum 155!
druggy_lane_solution.txt|druggy_lane.taf|You have managed to deal your way to freedom!
escape_from_insanity_solution.txt|Insane.taf|Congratulations psychopath, you're now a pyro.
lost_souls_solution.txt|lostsouls.taf|You don't want to go down there.
chicago_solution.txt|chicago.taf|Daisy was found guilty of double homicide
everything_solution.txt|everything.taf|I'll smile as I curse her name and everything Emanuelle.|SCR_SKIP_WAITKEY=1
textident_evil_solution.txt|Textident_Evil.taf|Congratulations! You've successfully beaten Textident Evil.|SCR_SEED=2
impulso_solution.txt|impulso.taf|Solo una cosa. Me di cuenta hace un cuarto de hora
ms_mobius_solution.txt|ms_mobius.taf|That little TV screen for the inside of your hat was a good investment.
morning_headache_solution.txt|A_Morning_with_a_Headache.taf|This has turned out to be an altogether OK morning.
sleaze_solution.txt|sleaze.taf|You scored 100 out of the maximum 100!
manor_solution.txt|manor.taf|You bury the crucifix with the other items.
lostmines_solution.txt|lostmines.taf|Congratulations, you have found the lost gold.
darktower_solution.txt|DarkTower.taf|restored power to the building.
report_solution.txt|report.taf|You scored 100 out of the maximum 100!
farfromhome_solution.txt|FarFromHome.taf|You scored 50 out of the maximum 50!|SCR_SKIP_WAITKEY=1
stardust_solution.txt|S_Tar_Dus.taf|You decide to go with the plant lady and
diarystrip_solution.txt|diarystrip.taf|You earn a huge tip and the ladies are all in love with you
silk_noil_solution.txt|SILKNOIL.TAF|The Silk King sprays his crotch liberally with a perfume that soon befouls
wheels_must_turn_solution.txt|Wheel105.taf|That is it, Twenty-Three.|SCR_SKIP_WAITKEY=1
asylum_solution.txt|as.taf|A large plaque sat on the wall|SCR_SKIP_WAITKEY=1
life_solution.txt|life.taf|Health=%health%|SCR_SKIP_WAITKEY=1
renuntio_solution.txt|Renuntio.taf|Yo-nos me alzo y estiendo mis-nos brazos|SCR_SKIP_WAITKEY=1
hhorror_solution.txt|hhorror.taf|It has been a long and frightful night|SCR_SEED=50 SCR_SKIP_WAITKEY=1
hhorror_patched_solution.txt|hhorror.taf|You scored 155 out of the maximum 155!|SCR_SEED=50 SCR_SKIP_WAITKEY=1 SCR_ASSUME_PATCHES=1
richard_solution.txt|Richard.taf|You scored 1000 out of the maximum 1000!|SCR_SKIP_WAITKEY=1
windy2_solution.txt|windy2.taf|You spin and see Liz running out of the woods towards you.
salutations_solution.txt|salutations.taf|you'll decline to answer.|SCR_SKIP_WAITKEY=1

iachini_solution.txt|iachini.taf|You settle down in front of the TV.|SCR_SEED=202
relojero_solution.txt|relojero.taf|Cierro los ojos y lloro.
vetknow_solution.txt|vetknow.taf|AND THE NEW WORLD CHAMPION IS|SCR_SKIP_WAITKEY=1
vetknow2_solution.txt|vetknow2.taf|AND THE NEW WORLD CHAMPION IS|SCR_SKIP_WAITKEY=1
losttomb_solution.txt|losttombv2.taf|you and Rupert start the trek back to camp.
journ2_solution.txt|Journ2.taf|You are carrying the King of Hearts.|SCR_SEED=2
journ2_patched_solution.txt|Journ2.taf|Your score is 30 out of a maximum of 90.|SCR_SEED=2 SCR_ASSUME_REPEATS=1 SCR_ASSUME_PATCHES=1
murder_great_falls_solution.txt|mudergreatfalls.taf|Ken is found guilty of triple homicide.|SCR_SKIP_WAITKEY=1
vampire_solution.txt|Vampire.taf|Your score is 70 out of a maximum of 100.|SCR_SKIP_WAITKEY=1
vampire_repeatassist_solution.txt|Vampire.taf|You scored 100 out of the maximum 100!|SCR_ASSUME_REPEATS=1 SCR_SKIP_WAITKEY=1
vampire_patched_solution.txt|Vampire.taf|You scored 100 out of the maximum 100!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1

merry_murders_solution.txt|Merry_Murders.taf|My score is 120 out of a maximum of 135.|SCR_SKIP_WAITKEY=1
merry_murders_repeatassist_solution.txt|Merry_Murders.taf|You scored 135 out of the maximum 135!|SCR_ASSUME_REPEATS=1 SCR_SKIP_WAITKEY=1
merry_murders_patched_solution.txt|Merry_Murders.taf|You scored 135 out of the maximum 135!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1

thewoods_solution.txt|thewoods.taf|You scored 100 out of the maximum 100!|SCR_SKIP_WAITKEY=1

captive_solution.txt|Captive.taf|You scored 100 out of the maximum 100!|

wonderwombat_solution.txt|wonderwombat.taf|THUMPER KICKS ASS!!!|SCR_SEED=3 SCR_SKIP_WAITKEY=1

vardock_bates_solution.txt|Vardock Bates.taf|HAS ELEGIDO LA INMORTALIDAD PARA SIEMPRE|SCR_SKIP_WAITKEY=1

croft_solution.txt|croft.taf|You scored 150 out of the maximum 150!
dr-who-vortex-lust_solution.txt|dr-who-vortex-lust.taf|You scored 150 out of the maximum 150!
gamma_solution.txt|gamma.taf|You scored 150 out of the maximum 150!
plunder_gargoyle_solution.txt|plunder_gargoyle.taf|Ye scored 10 out of the maximum 10!
albert_is_lost_solution.txt|Albert is Lost! An Adventure in Real Life.taf|Tiberius and Albert went home happily|SCR_SEED=42 SCR_SKIP_WAITKEY=1
target_solution.txt|target.taf|You managed to score 100 out of 100.|SCR_SEED=212
door_solution.txt|door.taf|You head south. You have escaped.
marlin_affair_solution.txt|marlin_affair.taf|The Marlin Affair: Chapter One|SCR_SKIP_WAITKEY=1
cibass_solution.txt|CIBASS.taf|[Press any key to discontinue]|SCR_SKIP_WAITKEY=1
pestilence_solution.txt|pestilence.taf|You managed to score 100 out of the maximum 100.
gmylm_solution.txt|GMYLM_2010.taf|Victory! - - -|SCR_SKIP_WAITKEY=1
provenance_solution.txt|provenance.taf|Look for PROVENANCE II in the summer of 2006!!!|SCR_SKIP_WAITKEY=1
provenance_patched_solution.txt|provenance.taf|You scored 300 out of the maximum 300!|SCR_SKIP_WAITKEY=1 SCR_ASSUME_PATCHES=1

professor_solution.txt|Professor.taf|You scored 154 out of the maximum 229!
professor_patched_solution.txt|Professor.taf|You scored 229 out of the maximum 229!|SCR_ASSUME_PATCHES=1
wingman1_solution.txt|wingman1.taf|You scored 95 out of the maximum 121!

newton_solution.txt|Newton.taf|u dscvr gravity
# Conversation With A Picture (2257 bytes, 4.00): one room, no score. Sit on
# the bench, ask the talking Picture NPC about "bird" (unlocks "parrot"),
# then ask about "parrot" to fire the win. 3 commands.
picture_solution.txt|Picture.taf|The title of the picture is "The Parrot's Cage".
smote_solution.txt|smote.taf|smote all 3 worlds into submission
rift_solution.txt|rift.taf|Thanks for playing this intro.
foggybanana_solution.txt|The Foggy Banana Adventure.taf|SPIDERS have been captured by you and sold to the
vault_solution.txt|The Vault.taf|And as if  the gods have answered you, the vault door begins to open.
pilfers_solution.txt|Pilfers.taf|You scored 107 out of the maximum 107!|SCR_SKIP_WAITKEY=1
witnessdemon_solution.txt|Witness_Demon_vs_Vampire.taf|You have saved your church from the horrors of the two monsters fighting over|SCR_SKIP_WAITKEY=1
stowaway_solution.txt|The_Stowaway.taf|Well done - you scored maximum points!
blast_solution.txt|blast.taf|You exit the building.  You have won!!!!
hiker_solution.txt|hiker.taf|You have found Ending Three of Three.
justanotherday_solution.txt|Just Another Day.taf|Congratulations...You won the game.|SCR_SKIP_WAITKEY=1
wayout_solution.txt|Way Out.taf|You're alive! But you'll never be the same...
flyhuman_solution.txt|The Fly Human.taf|Still... I guess this is the end.
zombiecow_solution.txt|zombiecow.taf|You are a free cow now.|SCR_SKIP_WAITKEY=1
raccoon_solution.txt|raccoon.taf|You dive headfirst into the can, easily shredding thin plastic bags with your|SCR_SKIP_WAITKEY=1
outline_solution.txt|outline.taf|Well done - you scored maximum points!
hungry_solution.txt|hungry.taf|Escape. Freedom.
longbarrow_solution.txt|longbarrow.taf|That'll show 'em (and maybe even bag you a raise).
# Asteroid Aftermath (single-hub satellite-realignment puzzle, no scoring):
# valve toggles silently relocate NPC satellites between camera rooms; a
# specific open/close sequence lands all required satellite groups together.
asteroidafter_solution.txt|asteroid_after.taf|All satellites correctly aligned.|SCR_SKIP_WAITKEY=1
existence_solution.txt|Existence.taf|Congratulations!  You've made it through the ADRIFT IntroComp 2009 version of|SCR_SKIP_WAITKEY=1
p2p_solution.txt|P2P.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
zacksmackfoot_solution.txt|zacksmackfoot.taf|THE END . . . . . for now!|SCR_SKIP_WAITKEY=1
boiledeggs_solution.txt|boiled eggs.taf|You summon the willpower to keep the box shut until you get home.
shufflingroom_solution.txt|The_Shuffling_Room.taf|your powerful discovery.
angeldevilhuman_solution.txt|The Angel the Devil and the Human.taf|Have a peanut.
herrdoktor_solution.txt|herrdoktor.taf|Mein tiny jetpack ist ein success!
rollingthedough_solution.txt|rollingthedough.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
murdermansionntro_solution.txt|MurderMansionntro.taf|Thank you for trying my Intro to Murder Mansion|SCR_SKIP_WAITKEY=1
whitterscap_solution.txt|whitterscap.taf|You win with the best score and stuff, yeah!
dangersdrivingnight_solution.txt|The Dangers of Driving at Night.taf|no longer bothering to hide his long, curved fangs|SCR_SKIP_WAITKEY=1
allhallowseve_solution.txt|All Hallows Eve.taf|You scored 23 out of the maximum 26!|SCR_SKIP_WAITKEY=1
gorxungula_solution.txt|gorxungula.taf|Elder Moose rouses from the depths of thought once the offering is in place.|SCR_SKIP_WAITKEY=1
lobster_solution.txt|lobster.taf|Next: WORLD DOMINATION!
businessasusual_solution.txt|Business As Usual.taf|You Won, Of Course
ohhuman_solution.txt|Oh_Human.taf|Congratulations!  You beat the game!
ohhuman_200_solution.txt|Oh_Human.taf|Your score is 200 out of a maximum of 200
sandy_solution.txt|Sandy.taf|You see no such thing.
sandy_patched_solution.txt|Sandy.taf|Yes! You finally got the doll!|SCR_ASSUME_PATCHES=1
sandy_meta_number_solution.txt|Sandy.taf|You see no such thing.
ptgood_solution.txt|competition2006__adrift__ptgood__PTGOOD.taf|You win! Yay!
phoneb_solution.txt|Phoneb.taf|Committing its final act of mercy
jinxtron_solution.txt|JINXTRON.taf|You're unjinxed now.
jinxtron_full_solution.txt|JINXTRON.taf|I'm free!  Bwa hahaha!|SCR_SEED=31
skydiver_solution.txt|The_Skydiver.taf|I'm almost dea-
theroad_solution.txt|the_road.taf|the knowledge of oblivion|SCR_SKIP_WAITKEY=1
perfectspy_solution.txt|The Perfect Spy.taf|Congratulations!  You have successfully escaped from the facility!
secidenoddcomp_solution.txt|seciden_oddcomp.taf|You scored 102 out of the maximum 102!|SCR_SKIP_WAITKEY=1
perspectives_solution.txt|perspectives.taf|Congratulations, you achieved the Negotiation Style Ending!|SCR_SKIP_WAITKEY=1
bigcitylaundry_solution.txt|Big City Laundry.taf|Congratulations!  You've done it.
overtheedge_solution.txt|Over the Edge1.0.taf|the End|SCR_SKIP_WAITKEY=1
drinks_solution.txt|Drinks.taf|THE END.|SCR_SKIP_WAITKEY=1
r2dc_solution.txt|R2DC.taf|You scored 1000000 out of the maximum 1000000!|SCR_SKIP_WAITKEY=1
foresthouse2_solution.txt|TheForestHouse_2.taf|You scored 13 out of the maximum 12!|SCR_SKIP_WAITKEY=1
shetland_solution.txt|The_Shetland_Enigma.taf|distress broadcaster|SCR_SKIP_WAITKEY=1
takeone_solution.txt|takeone.taf|it only took 1 take|SCR_SKIP_WAITKEY=1
tenebraesemper_solution.txt|TenebraeSemper.taf|You take the loaded pistol from Lauren's dresser.|SCR_SKIP_WAITKEY=1
tenebraesemper_patched_solution.txt|TenebraeSemper.taf|[Ending 4 of 5]|SCR_SKIP_WAITKEY=1 SCR_ASSUME_PATCHES=1
helsing_solution.txt|Helsing.taf|But not tonight|SCR_SKIP_WAITKEY=1
worstgame_solution.txt|WorstGameInTheWorld.taf|Another game then? If you dare?|SCR_SKIP_WAITKEY=1
spooked_solution.txt|Spooked_The_Wonders_of_Science.taf|Congratulations you won!|SCR_SKIP_WAITKEY=1
videotapedecay_solution.txt|Video_Tape_Decay.taf|And fade to white.|SCR_SKIP_WAITKEY=1
regrets_solution.txt|Regrets.taf|The game has ended.|SCR_SKIP_WAITKEY=1
terrified_solution.txt|Terrified.taf|The game has ended and you have won!|
terrified_patched_solution.txt|Terrified.taf|Your score is 65 out of a maximum of 65|SCR_ASSUME_PATCHES=1
rain_solution.txt|rain.taf|Lightning bolts away to Thunder's earsplitting roar of triumph.|SCR_SKIP_WAITKEY=1
howitstarted_solution.txt|howitstarted.taf|You scored 6 out of the maximum 6!|
stationxiii_solution.txt|Station_XIII.taf|To be continued...|
choosethreehour_solution.txt|Choose_Your_Own_Three_Hour_Adventure.taf|Overall, you got a score of 9 out of a maximum possible 14.|SCR_SKIP_WAITKEY=1
thelasthour_solution.txt|thelasthour.taf|"Here we are... MY BROTHER."|
sexismental_solution.txt|Sex is Mental.taf|Where's that broken Glass?|
petespunkin_solution.txt|Pete's Punkin Junkinator.taf|You scored 505 out of the maximum 575!|
crookedestate_solution.txt|The Crooked Estate.taf|I quit momentarily, lying motionless, without any will. But, still, something|SCR_SKIP_WAITKEY=1
aliasagent_solution.txt|Alias Undercover Agent.taf|You scored 35 out of the maximum 35!|
viewtohome_solution.txt|A View to a Home.taf|Congratulations! You have collected all three medals!|
briefcase_solution.txt|briefcase.taf|[The end]|
theseance_solution.txt|The_Seance.taf|Towards eternity with your love...|SCR_SKIP_WAITKEY=1
reactor1_solution.txt|reactor_1.taf|Congratulations, You saved the ship!|SCR_SEED=1 SCR_SKIP_WAITKEY=1
motion_solution.txt|Motion.taf|You scored 100 out of the maximum 100!|SCR_SEED=1041 SCR_SKIP_WAITKEY=1
tophat_solution.txt|tophat.taf|But will the next show go the same way?|
threeminutes_solution.txt|3 minutes1.0.taf|But not a hero anymore.|SCR_SEED=3 SCR_SKIP_WAITKEY=1
neighbours_solution.txt|neighbours.taf|Well done indeed!|SCR_SKIP_WAITKEY=1
firstpug_solution.txt|The First To Arise Alone With A Pug.taf|You scored 100 out of the maximum 100!|
foresthouse3_solution.txt|ForestHouse3.taf|between your gorgeous wife and beautiful son, you find that you are|SCR_SKIP_WAITKEY=1
dayattheoffice_solution.txt|DayAtTheOffice.taf|I'll have a tea, black with two sugars and don't stinge on the water.|SCR_SKIP_WAITKEY=1
beer_solution.txt|beer.taf|You search the dirt and find a pouch.|
fluffykins_solution.txt|Mr_Fluffykins_Most_Harrowing_Misadventure.taf|Congratulations, Mr. Fluffykins! And you too, reader!|SCR_SKIP_WAITKEY=1
witchtale_solution.txt|A Witch Tale.taf|THE END|SCR_SKIP_WAITKEY=1
doortoutopia_solution.txt|Door to Utopia, The.taf|Up to the last sentence, you thought you really were in Hell, but the words|SCR_SKIP_WAITKEY=1
patient7_solution.txt|Patient7.taf|Your world.|SCR_SKIP_WAITKEY=1
finalquestion_solution.txt|The_Final_Question.taf|And there ends The Final Question|SCR_SKIP_WAITKEY=1
mustescape_solution.txt|mustescape.taf|Not that your boss will like that.|SCR_SKIP_WAITKEY=1
caidalibre_solution.txt|Caida libre.taf|You puntosd 0 fuera of the maximum 0!|
templeofthesun_solution.txt|Temple_Of_The_Sun.taf|It is good to see that you didn't lose your head about this!|SCR_SEED=2 SCR_SKIP_WAITKEY=1
wolvesatthedoor_solution.txt|Wolves_at_the_Door.taf|And on that somewhat sombre note, Wolves At The Door is concluded.|SCR_SKIP_WAITKEY=1
apokalupsis_solution.txt|apokalupsis.taf|Thank you for playing the introduction to Apokalupsis.|SCR_SKIP_WAITKEY=1
dusk_solution.txt|dusk.taf|Most importantly you finally saw a tree frog!|
thehunter_solution.txt|The_Hunter.taf|You scored 50 out of the maximum 50!|
will_solution.txt|Will.taf|Well done - you scored maximum points!|
cobl_solution.txt|COBL.taf|Your score is 160 out of a maximum of 230.  (69%)|SCR_SEED=28 SCR_SKIP_WAITKEY=1
cobl_patched_solution.txt|COBL.taf|You scored 230 out of the maximum 230!|SCR_SEED=28 SCR_SKIP_WAITKEY=1 SCR_ASSUME_PATCHES=1
puzzlebox_solution.txt|puzzlebox.taf|shouts, "Get out and stay out!"  The door slams shut.  You are free!|SCR_SEED=1
amy_solution.txt|amy.taf|You've reached the end of this adventure - congratulations.|SCR_SKIP_WAITKEY=1
cluelessbob_solution.txt|In_the_Claws_of_Clueless_Bob.taf|score of 12 - well done. You didn't resort to the hints at all. If not, maybe|SCR_SKIP_WAITKEY=1
hub_solution.txt|hub.taf|driveway, and take off down the suburban street, not once looking back.|
ynkaboom_solution.txt|YNKaboom.taf|***WHOA!  YOU TOTALLY JUST WON!***|
bsg22_solution.txt|BSG TWENTY TWO Final.taf|YOU HAVE WON!|SCR_SEED=2 SCR_SKIP_WAITKEY=1
jailbreakbob_solution.txt|jailbreakbob.taf|woo-hoo!|SCR_SKIP_WAITKEY=1
dreamquest_solution.txt|Dream Quest.taf|Well done - you scored maximum points!|
wilkins_solution.txt|The_Strange_Tale_of_Dr_Wilkins.taf|My score is 114 out of a maximum of 95.|
darkness_solution.txt|darkness.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
backhome_solution.txt|Back Home.taf|You are back.  Back home.|SCR_SKIP_WAITKEY=1
zelda_solution.txt|zelda.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
showtime_solution.txt|Showtime_at_the_Gallows.taf|I will STRIKE like the fucking Hand Of God.|SCR_SKIP_WAITKEY=1
oldchurch_solution.txt|The Old Church.taf|Axel Gyllenpil has come to peace after a sermon held in the chapel.|SCR_SKIP_WAITKEY=1
suzypowers_solution.txt|competition2011__adrift__powers__how suzy got her powers.taf|You achieved this with a score of 22 out of a maximum possible of 22.|SCR_SKIP_WAITKEY=1
rockband_solution.txt|Rock Band.taf|You did it! You stopped Gigantor and saved the world (and Rock Band!)|SCR_SEED=1 SCR_SKIP_WAITKEY=1
aegis_solution.txt|Aegis.taf| END|SCR_SKIP_WAITKEY=1
warlock_solution.txt|warlock.taf|The laboratory stands empty. No sign of demonic presences save a chalk-drawn|
escapepod_solution.txt|EscapePod.taf|Well done - you scored maximum points!|
handyman_solution.txt|Handyman.taf|You scored 200 out of the maximum 200!|
xclue_solution.txt|xclue1.0a.taf|THE END|
ovaloffice_solution.txt|ovaloffice.taf|You scored 32 out of the maximum 32!|
planescape_solution.txt|Planescape-Encounters1.taf|You scored 15 out of the maximum 15!|
studio_solution.txt|studio.taf|You scored 100 out of the maximum 100!|
studio_patched_solution.txt|studio.taf|You scored 100 out of the maximum 100!|SCR_ASSUME_PATCHES=1
funtown_solution.txt|fun town.taf|Sorry, there is no way to open the chest before you accumulate 200 points.|
funtown_patched_solution.txt|fun town.taf|Well done - you scored maximum points!|SCR_ASSUME_PATCHES=1
rodneyprincess_solution.txt|RodneyandthePrincess40v3-1.taf|Congratulations on getting your reward!|
salvation_solution.txt|salvation.taf|Thanks for playing.|
christmaspresent_solution.txt|christmas present 1.0.taf|Thank you for playing.  Merry Christmas and  a Prosperous New Year!|
digby_solution.txt|For_Love_of_Digby.taf|For Love Of Digby winds to a close.|SCR_SKIP_WAITKEY=1
sigurd_solution.txt|Sigurd_Fafnesbane.taf|gold is glittering in the leather sack in your luggage.|SCR_SKIP_WAITKEY=1
unfortunately_solution.txt|Unfortunately.taf|An ending to be sure - and the best one in the game to boot!|SCR_SKIP_WAITKEY=1
frustrated_solution.txt|frustrated.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
camelot15_solution.txt|Camelot 1,5.taf|Looks like the old Merlin did read your mind correctly after all.|SCR_SKIP_WAITKEY=1
jimpond_solution.txt|JimPond.taf|and I'll be wanting you to lead the attack.|SCR_SKIP_WAITKEY=1
greekschool_solution.txt|Greek School Adventure.taf|You scored 185 out of the maximum 275!|
trickortreat_solution.txt|Trick or Treat.taf|You flee to freedom.|SCR_SKIP_WAITKEY=1
volant_solution.txt|volant.taf|You have won! Good for you!|
deardiary_solution.txt|Dear Diary.taf|Boy, I needed to get that off my chest.|
riding_home_solution.txt|Riding_Home.taf|You have won "Riding Home."|SCR_SKIP_WAITKEY=1
deardiary2_solution.txt|Dear Diary 2.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
fullcircle_solution.txt|Full_Circle.taf|Full Circle has ended.|SCR_SEED=2 SCR_SKIP_WAITKEY=1
halloweenhijinks_solution.txt|HalloweenHijinks.taf|Well done! You've reached the best ending in the game!|SCR_SKIP_WAITKEY=1
barneysproblem_solution.txt|BarneysProblem.taf|BABYLON|SCR_SKIP_WAITKEY=1
deadreckoning_solution.txt|DeadReckoning.taf|this is the best of the lot. Well done indeed!|SCR_SKIP_WAITKEY=1
cldone_solution.txt|cldone.taf|Well done - you scored maximum points!|SCR_SEED=3
scandal_solution.txt|Scandal.taf|Admiral Byng resigns from his|SCR_SEED=1
bloodrelatives_solution.txt|Blood_Relatives.taf|you did find your bed|
bloodrelatives_patched_solution.txt|Blood_Relatives.taf|Using the big bronze key, you open the door.|SCR_ASSUME_PATCHES=1
paint_solution.txt|Paint.taf|Well done! You've reached the best ending in the game!|SCR_SKIP_WAITKEY=1
hcw_solution.txt|hcw.taf|Well, you didn't conquer the world today. But there's always tomorrow.|SCR_SEED=2 SCR_SKIP_WAITKEY=1
yadfa_solution.txt|YADFA.TAF|gained yourself a nice (haunted) castle. Not bad for a day's work.|SCR_SKIP_WAITKEY=1
requiem_solution.txt|competition2006__adrift__requiem__requiem.taf|you have reached the game's best ending|SCR_SKIP_WAITKEY=1
mindofmaster_solution.txt|competition2007__adrift__mindofmaster__mind of master.taf|You are victorious, whoever you might|SCR_SKIP_WAITKEY=1
withoutaclue_solution.txt|WithoutAClue.taf|you've managed to finish the game|SCR_SKIP_WAITKEY=1
cowboyblues_solution.txt|CowboyBlues.taf|how does it feel to be a hero then, Fingle Bodge?|
grumble_solution.txt|Whatever_Happened_to_Uncle_Grumble.taf|Your score is 226 out of a maximum of 404|SCR_SKIP_WAITKEY=1
magicshow_solution.txt|magicshow.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
goblin_solution.txt|goblin.taf|Oh, and before we forget- Congratulations, gobbo. Or, should we say...|
mould_solution.txt|mould.taf|Congratulations on winning The Potter and the Mould|SCR_SEED=1 WINE_FEED_NO_HINTS=1
blood_solution.txt|blood.taf|You managed to score 140 out of 140.|SCR_SKIP_WAITKEY=1
rking_solution.txt|rking.taf|Overall, your score was 100 out of a total of 100.|SCR_SKIP_WAITKEY=1
hero_solution.txt|competition2004__adrift__hero__hero.taf|the world is a better place for your actions|SCR_SKIP_WAITKEY=1
datewithdeath_solution.txt|datewithdeath.taf|And you have a whole life ahead of you to live|SCR_SKIP_WAITKEY=1
alchemist_solution.txt|alchemist.taf|That is 100% of the game|SCR_SKIP_WAITKEY=1
onnafa_solution.txt|ONNAFA.TAF|your score turned out at 82|SCR_SKIP_WAITKEY=1
house_solution.txt|House.taf|Well done - you scored maximum points!|SCR_SEED=2 SCR_SKIP_WAITKEY=1
lca_solution.txt|Lights_Camera_Action.taf|best ending in the game!|SCR_SKIP_WAITKEY=1
# Re-blessed 2026-09-13 for the 4.0 event "ticked" byte and post-execute-task event
# check; identical to Adrift_1090_mutaydid.txt under SCR_RNG=xoshiro.
mutaydid_solution.txt|mutaydid.taf|That is 100% of the game|SCR_SKIP_WAITKEY=1
seaside_solution.txt|ADayAtTheSeaside.taf|Well done - you scored maximum points!
reluctantvampire_solution.txt|The_Reluctant_Vampire.taf|you achieved a score of 103 out of a possible of|SCR_SEED=6 SCR_SKIP_WAITKEY=1
sswhore_solution.txt|ss whore.taf|You scored 7 out of the maximum 7!|SCR_SKIP_WAITKEY=1
warlord_solution.txt|warlord.taf|you've successfully completed The Warlord,|SCR_SEED=6 SCR_SKIP_WAITKEY=1
tictactoe_solution.txt|Tic-Tac-Toe.taf|Congratulations, you won!|
elascensor_solution.txt|El ascensor.taf|tornillo cae al suelo|SCR_SKIP_WAITKEY=1
whitesingularity_solution.txt|The White Singularity.taf|You've gone to the core, saved the world, earned a Nobel prize with|
igor_solution.txt|igor.taf|You are now REDUNDANT ! !|
suburbanprodigy3_solution.txt|MikeDesert_SuburbanProdigy3.taf|Well done - you scored maximum points!|
egghunt_solution.txt|Egg_Hunt.taf|You scored 950 out of the maximum 1000!|
bandera_solution.txt|Bandera.taf|Well done - you puntosd maximum points!|SCR_SKIP_WAITKEY=1
wumpusrun_solution.txt|competition2006__adrift__wumpusrun__wumpusRun.taf|You have earned the right to the title|SCR_SEED=72
ilgolem_solution.txt|Il Golem.taf|Complimenti, hai completato l'avventura!|SCR_SKIP_WAITKEY=1
ghosttown_solution.txt|Ghost town v1,05.taf|Slowly two figures are seen shimmering in the air. One of a pretty young girl|SCR_SKIP_WAITKEY=1
sere_solution.txt|S.E.R.E.taf|You scored 215 out of the maximum 260!|SCR_SKIP_WAITKEY=1
freedom_solution.txt|escape.taf|Final score: 100% COMPLETED.|SCR_SKIP_WAITKEY=1
pathway_solution.txt|pathway.taf|and stop the test. Do you remember anything?|SCR_SKIP_WAITKEY=1
missingperson_solution.txt|MissingPerson.taf|sadness of the little girl?|
outside_solution.txt|Outside.taf|There is no more. What did she do?|
melancholy1_solution.txt|Melancholy Blood Act 1.taf|Well, that's the end of Act 1.|
professional_solution.txt|Professional.taf|Look forward to The Professional 2|SCR_SKIP_WAITKEY=1
namiki_solution.txt|Namiki'sDay.taf|You scored 8 out of the maximum 8!|
akari_solution.txt|AkarisStory.taf|You scored 13 out of the maximum 13!|
crossworlds2_solution.txt|Crossworlds Part 2.taf|You scored 75 out of the maximum 75!|SCR_SKIP_WAITKEY=1
crossworlds4_solution.txt|Crossworlds Part 4.taf|You scored 100 out of the maximum 100!|SCR_SKIP_WAITKEY=1
britishfox_solution.txt|British.Fox.and.the.Celebrity.Abductions.taf|You scored 46 out of the maximum 50!|SCR_SKIP_WAITKEY=1
doa_xbs_solution.txt|DOA_X_B_S.taf|You scored 10 out of the maximum 10!|
silvermaiden_solution.txt|The Silver Maiden.taf|You cast your ultimate spell at Velle.|
insidejob_solution.txt|insidejob.taf|You scored 614 out of the maximum 14198!|
aquarius1_solution.txt|AquariusPart1Ver1.taf|......To Be Continued........|
trappedwithagirl_solution.txt|trappedwithagirl.taf|There is a huge explosion.|
practicepolicy_solution.txt|practice policy.taf|You scored 330 out of the maximum 330!|SCR_SKIP_WAITKEY=1
makeshift_solution.txt|makeshift-magician.taf|your magic career is probably over. Well, it's probably a good thing...|SCR_SKIP_WAITKEY=1
aquarius2_solution.txt|AquariusPart2Ver1.taf|You scored 200 out of the maximum 200!|
sbwd2_solution.txt|sbwdII.taf|follow soon!|
chasingrussian_solution.txt|ChasingTheRussian_noSound.taf|You've completed this chapter of the adventure.|SCR_SKIP_WAITKEY=1
nem_solution.txt|nem.taf|Congratulations! Well done good and faithful servant!|
tobeking_solution.txt|To Be King v1.8.taf|YOU WIN!|
haremprologue_solution.txt|harem prologue.taf|Congratulations! You have completed the Prologue to the Tale of the Unlikely|
duchess_solution.txt|Duchess of Desire.taf|Suddenly the Duchess appears.|
fairscare_solution.txt|fairscarenightmare.taf|Now you can live in relaxation.|SCR_SKIP_WAITKEY=1
sceneofthecrime_solution.txt|Scene of the Crime.taf|McClane got you fair and square.|SCR_SKIP_WAITKEY=1
# Scene of the Crime 2: City In Fear.  Sit in Slorb's office after the Alice
# showdown (give bun, shoot alice x4).  Cartoon combat with a nine-year-old
# gangster, not sexual.  No i cheat.  280/550.  Waitkeys.
cityinfear_solution.txt|CityInFear.taf|Put it there... Captain|SCR_SKIP_WAITKEY=1
# Labyrinth.  xoshiro layout: snare in One Big Empty (disarm with knife),
# pit in Low Cave (amber stone), arrow in the vault (toolkit), singing
# sword in the Narrow Passage, then pedestal.  Magic trap is the bog (skip).
labyrinth_solution.txt|labyrinth.taf|The ceiling above bursts into light|SCR_SKIP_WAITKEY=1
bdw_solution.txt|BDW.taf|WHOO!  WHOO!  WHEE!  WHAA!|
advent350b_solution.txt|advent350b.taf|In that game you scored 350 out of a possible 350|SCR_SKIP_WAITKEY=1
# Adventure Strikes When You Least Expect It (tiny 4.00 puzzle).  Harden
# cheese with varnish, hook the crowbar through the bathroom window, saw
# the front door, set off the hallway fire alarm.  One-room-comp scale.
adventstrikes_solution.txt|AdventStrikes.taf|found your way out.|SCR_SKIP_WAITKEY=1
# Jason Evans 1 (horror, not AIF).  Milk/statue, Elm-Street candles, toaster
# fire into the hunters cabin (author stub `rtufghsdfvg` starts the blaze),
# castle box/hanged man, rocks at passing cars.  Waitkeys throughout.
jasonevans_solution.txt|jasonevans.taf|The town that is gone forever.|SCR_SKIP_WAITKEY=1
# Jason Evans 2: Misunderstood.  Author hints: pipe, hammer/door, clock axe,
# diner phone, knock, wait for the police walk, parking-lot note, hotel
# grandmother rope, then the rope on Jason in the church.  Waitkeys.
jason2_solution.txt|jason2.taf|Your fright factor is|SCR_SKIP_WAITKEY=1
# Jason Evans 3: Return.  Take the body, keys from the car floor, wrench in
# the woods trunk, wrench-fight, dark-room search, gun from the cabinet,
# shoot Jason, east.  125/125.  Waitkeys.
jason3_solution.txt|jason3.taf|You scored 125 out of the maximum 125!|SCR_SKIP_WAITKEY=1
# Jason Evans 4 (horror, not AIF).  Nightmare hotel, aunt's house, mansion
# key/vase, hammer/wall, crowbar under the secret-bedroom sheets, attic
# chest twice, lure Eli to the guillotine, then out.  Intro waitkeys.
jason4_solution.txt|Jason_4.taf|Like the prophet Eli did.....|SCR_SKIP_WAITKEY=1
# Dead Race (IntroComp 2009 demo).  Bedroom window is the whole game.
deadrace_solution.txt|deadRace.taf|this is just a demo|SCR_SKIP_WAITKEY=1
# Sigmund Praxis, Guerrilla Therapist (IntroComp 2002).  Wait for the
# Countess; any later command EndGames the teaser.
praxis_solution.txt|praxis.taf|only one of them can be the victor|SCR_SKIP_WAITKEY=1
# Crashland (Twin Comp 1st).  Sell the recycling plans, pay Maita to
# haul/fix the Obispo, buy grain deeds, board.  Dino/dust events are
# scenery.  Waitkeys.
crashland_solution.txt|crashlite.taf|Not a bad ending, after all|SCR_SKIP_WAITKEY=1
shadowjack_solution.txt|shadowjack.taf|** You have won the game! **|SCR_SKIP_WAITKEY=1
# Yon Castle intro.  Climb the nut tree, open the wall contraption so
# the drawbridge falls, climb down, east.
yoncastle_solution.txt|yoncastle_intro.taf|Congratulations on yon stunning reaching ye end|SCR_SKIP_WAITKEY=1
# The Magician's Niece (IntroComp 2009).  Teen mage, non-sexual.  PDA
# and magic bag, wait for Uncle's palantiri summons, west then goto the
# hall.  No EndGame; marker is the briefing close.
magiciansniece_solution.txt|The Magician's Niece.taf|Godspeed, Ariana|SCR_SKIP_WAITKEY=1
mysterymanor_solution.txt|mysterymanor.taf|You scored 200 out of the maximum 200!|SCR_SKIP_WAITKEY=1
selmaswill_solution.txt|SelmasWill.taf|You did a great job!|SCR_SKIP_WAITKEY=1
mm2_solution.txt|Monster in the Mirror 2.taf|I hope you enjoyed these short games|SCR_SKIP_WAITKEY=1
# Cloak of Darkness (3.80).  Hang the cloak before the bar message.
cloak_solution.txt|cloak.taf|*** You have won ***|SCR_SKIP_WAITKEY=1
# Bounty Hunter.  Gun, Roddy, bottle-for-ticket, vent, Donny, Jake.
bountyhunter_solution.txt|bountyhunter.taf|You scored 30 out of the maximum 30!|SCR_SKIP_WAITKEY=1
camelot_solution.txt|Escape_from_Camelot.taf|You scored 5 out of the maximum 5!|SCR_SKIP_WAITKEY=1
# Cumberbund (1_axia.taf). Search the three hiding places; the end-game
# event fires once var0 hits 3. Waitkeys on the closer.
cumberbund_solution.txt|1_axia.taf|twelve years ago and you killed her|SCR_SKIP_WAITKEY=1
# Where Am I? / Jack.taf. Tape off the hanging panel, cover the sensor,
# then the floor hole. Waitkeys. Named whereami so a filter cannot also
# hit shadowjack_solution.txt (which contains the substring jack_solution).
whereami_solution.txt|Jack.taf|You're FREE!|SCR_SKIP_WAITKEY=1
# The Lab Experiment. Passcard and gum in the lost-and-found cabinet,
# rock from the vault, charge the laser generators, press the booth
# button, gum the pressure hole. Waitkeys.
laboratory_solution.txt|laboratory.taf|Find out what happens in "Escape from the Lab".|SCR_SKIP_WAITKEY=1
# The Big Spy Fiction ch.1. ATM cash, buy wine, send the guard to the
# diner, cut the lock-gadget out of the clothes with mall scissors.
bigspy1_solution.txt|bigspy1.taf|Kindly run the second chapter|SCR_SKIP_WAITKEY=1
heist_solution.txt|heist.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
# Office Breakout. Coffee + holepunch through door/vent/elevator, then
# pepsi+coffee on the lobby fire. Full 60/60. Waitkeys on the closer.
officebreak_solution.txt|officebreak.taf|You scored 60 out of the maximum 60!|SCR_SKIP_WAITKEY=1
# The Big Spy Fiction ch.2. Starts in the lobby; distract the henchman
# with the sliding puzzle, open the office, crack the safe. Waitkeys.
bigspy2_solution.txt|bigspy2.taf|It's again time to play the next chapter|SCR_SKIP_WAITKEY=1
# Find Andy Part 1. Starts in the hotel bathroom. Flush the membership
# card, Ray's jelly riddle for the cake, Maggy then Vicky then Marjorie.
# Full 1000/1000. Waitkeys.
findandy1_solution.txt|findandy1.taf|You scored 1000 out of the maximum 1000!|SCR_SKIP_WAITKEY=1
# The Dark River. Key under the rocks, 60 m left (13.5 m/s * ~4.4 s),
# then max power. Full 12/12. Waitkeys in the closer.
darkriver_solution.txt|The_Dark_River_1.4.taf|You scored 12 out of the maximum 12!|SCR_SKIP_WAITKEY=1
lockedout_solution.txt|lockedout.taf|You scored 90 out of the maximum 110!|SCR_SKIP_WAITKEY=1
lockedout_patched_solution.txt|lockedout.taf|You scored 110 out of the maximum 110!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1
# The Big Spy Fiction ch.3. Ask the prisoner, bone the henchman, call the
# lawyer, then mace and the lair's self-destruction. Waitkeys.
bigspy3_solution.txt|bigspy3.taf|Narrator can not ask whether this is end of our hero|SCR_SKIP_WAITKEY=1
# Goldilocks B&E. Keypad 667, spoons after porridge, fleece, tabasco on
# the padlock and bars, rope out the bedroom. 32/35 (library GET takes
# the fridge egg before the +2 task). Waitkeys.
goldbe_solution.txt|Gold_B_and_E_v1.2.taf|You scored 32 out of the maximum 35!|SCR_SKIP_WAITKEY=1
goldbe_patched_solution.txt|Gold_B_and_E_v1.2.taf|You scored 35 out of the maximum 35!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1
# Death House. Sit to play the organ, steel key for the attic box, rusty
# for the cabinet; wait out the graveyard cycle; Charles then the baby
# auto-take their gifts. Full 42/42. Waitkeys.
deathhouse_solution.txt|deathhouse.taf|You scored 42 out of the maximum 42!|SCR_SKIP_WAITKEY=1
heretoday_solution.txt|heretoday.taf|The band rips into "Tidal Wave"|SCR_SKIP_WAITKEY=1
raiders_solution.txt|raiders.taf|Hurray you have beat the evil monster valludu|SCR_SKIP_WAITKEY=1
# Heroes. Drink the pub beer, fly the boots, warehouse alien, hayloft orb.
heroes_solution.txt|heroes.taf|the least we could do, Rob|SCR_SKIP_WAITKEY=1
# Postman Matt. Pen under the body, sign Bottomley's letter. Waitkeys.
postmanmatt_solution.txt|Postman Matt.taf|Your rank is masterful|SCR_SKIP_WAITKEY=1
# Lab Rats. Vent, unseal, slide in, reseal, pump, focus. Waitkeys.
labrats_solution.txt|labrats.taf|This is just what we've been looking for.|SCR_SKIP_WAITKEY=1
# Shelter. Bolt from the chest into the slab, rope over the hook, shelves.
shelter_solution.txt|shelter.taf|falling atomic bombs|SCR_SKIP_WAITKEY=1
tvcaper_solution.txt|tvcaper.taf|Rockin' man!|SCR_SKIP_WAITKEY=1
dragonsphere_solution.txt|dragon sphere v2.0.taf|You have found the Dragon Sphere and destroyed the Evil|SCR_SKIP_WAITKEY=1
# The Isle. Name/gender, sword for Johnnie, flowers for Sarah, gold for
# the boat. Full 1500/1500.
theisle_solution.txt|The_Isle.taf|You scored 1500 out of the maximum 1500!|SCR_SKIP_WAITKEY=1
# The Mansion's Mystery intro. Street to the front door.
tmm_solution.txt|T._M._M.taf|You won!|SCR_SKIP_WAITKEY=1
# Shanilor demo. Key in the mirror-chest, unlock the shutter, climb down.
shanilor_solution.txt|shanilor.taf|You have completed this demo.|SCR_SKIP_WAITKEY=1
# Stalker. Holepunch the window, fridge, windex, phone then screwdriver. 60/60.
stalker_solution.txt|stalker.taf|fallen asleep watching horrors movies again|SCR_SKIP_WAITKEY=1
# Ispace. Hammer from the living room, hit the walnut jar.
ispace_solution.txt|Ispace.taf|worldwide craze|SCR_SKIP_WAITKEY=1
# Sad Obsession. Mega chilli to Hell, lax coffee, holy water, pizza 9377 3591.
obsession_solution.txt|obsession.taf|Congratulations, you fulfilled your|SCR_SKIP_WAITKEY=1
# Ziva.taf (AIF): NCIS adult; solution/golden gitignored.
ziva_solution.txt|Ziva.taf|You scored 240 out of the maximum 240!|SCR_SKIP_WAITKEY=1
# Rocky Raccoon. Fish for the cat, attack Dan and Nancy, sewer escape.
rocky_solution.txt|Rocky_Raccon_Game.taf|safely rescued your family|SCR_SKIP_WAITKEY=1
smokedemo_solution.txt|Smoke Demo.taf|You scored 45 out of the maximum 45!|SCR_SKIP_WAITKEY=1
# Homeless Harry (AIF): 7/7. Hand Willy the skirt BEFORE the Lysol (the
# Lysol sends him into the box, out of reach of the skirt task), then the
# Lysol and the ending. Gitignored goldens.
homelessharry_solution.txt|Homeless Harry.taf|Yay, you won the game!|SCR_SKIP_WAITKEY=1
gosha_solution.txt|gosha.taf|You scored 250 out of the maximum 250!|SCR_SKIP_WAITKEY=1
# Matt's Strange Adventure. Typed combat verbs; n into the temple; Room B east then boss.
matt_solution.txt|matt.taf|You scored 161 out of the maximum 161!|SCR_SKIP_WAITKEY=1
# Sentor. Lighter, statue-key, fire dragon, torch, Kali knife. 13/13: T2
# "slap Stefcho", which 3.90 rewrites to hit before task matching, runs
# through the line's other spelling (spelling_task).
sentor_solution.txt|sentor.taf|You became a favourite man of KALI|SCR_SKIP_WAITKEY=1
# Marmalade Skies. Suit, bar-vault columns, recode transmitter. 150/150.
marmalade_solution.txt|Marmalade_Skies.taf|You have been saved.|SCR_SKIP_WAITKEY=1
# The Crash. Bandage, ski the cliff, nails, boots off the body, patch the boat.
thecrash_solution.txt|thecrash.taf|Your cell phone rings!|SCR_SKIP_WAITKEY=1
locuras_solution.txt|Locuras.taf|You scored 175 out of the maximum 200!|SCR_SKIP_WAITKEY=1
# home1-2.taf (AIF): Faith; 163/163 max. After the main scene, open the
# blouse (two of the scene tasks require it open) and play the ten small
# scene tasks (each pays once), then lie on bed and sleep. Gitignored goldens.
home12_solution.txt|home1-2.taf|You scored 163 out of the maximum 163!|SCR_SKIP_WAITKEY=1
startrek1_solution.txt|thestartrekchainreactionepisode1thehornetsnest.taf|Elevator malfunction.|SCR_SKIP_WAITKEY=1
# Vengance (AIF). Gitignored goldens.
vengance_solution.txt|Vengance.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
# Alex / Heather lab. Kiss, straps, mix green antidote. Gitignored goldens.
alex_solution.txt|alex.taf|You did it! You're safe now!|SCR_SKIP_WAITKEY=1
# Notice Me (AIF). Rose, eavesdrop, Tanith package, pet, kiss Lethe. Gitignored.
noticeme_solution.txt|Notice Me.taf|You scored 50 out of the maximum 50!|SCR_SKIP_WAITKEY=1
# Urban Dragon. Crowbar, cold beer for Gus, open the dumpster, warrant,
# houseboat ice, shoot Baxter. 130/130.
urbandragon_solution.txt|urbandragon.taf|afford your own houseboat|SCR_SKIP_WAITKEY=1
# Stainless Steel Rat (rat.taf). Cop/button, board, ticket punch, armored car, Inskipp.
rat_solution.txt|rat.taf|Congratulation! You have successfully completed the first part|SCR_SKIP_WAITKEY=1
ghost_solution.txt|ghost.taf|As if you could ever forget.|SCR_SKIP_WAITKEY=1
# Wasteland bunker teaser. Keys, start the truck, winch the blast doors, west.
wasteland_solution.txt|wasteland.taf|shadow of tall mountains|SCR_SKIP_WAITKEY=1
# Ginger (AIF). EVENT after the explicit task. Gitignored goldens.
ginger_solution.txt|Ginger.taf|Yep, when it rains it pours.|SCR_SKIP_WAITKEY=1
# Spooked 2. Rope-rock, 212188 trunk, hook+dresser, ask then get head.
spooked2_solution.txt|Spooked2.taf|Congratulations you won with a score of 23 out of 23!|SCR_SKIP_WAITKEY=1
# Time Adventure. Crackers+engine, salted gang, armor, sword, fork, wired battery. 175/175.
timeadventure_solution.txt|timeadventure.taf|It must have all been a nightmare|SCR_SKIP_WAITKEY=1
# The Cell. Rug-key, paperclip, follow Eagle, pipe, red teleporter, chip in jacket.
cell_solution.txt|cell.taf|Congratulations, you won!|SCR_SKIP_WAITKEY=1
# Breakout (AIF). Oil+closet strips, bucket torch, alarm, Klygana scene (all
# scene tasks; T66/T67 must precede T65, chair needs T65), chair. 100/100.
breakout_solution.txt|breakout.taf|Congratulations - you've won the game!|SCR_SKIP_WAITKEY=1
# Merlin Bird of Prey IntroComp teaser. Pull chain, bronze doors, fountain.
merlin_solution.txt|The Merlin Bird of Prey - IntroComp.taf|monks appear at the grating|SCR_SKIP_WAITKEY=1
# drifting.taf. Mouse in drawer, modem delivery, forum, Hintmaster, Correct One.
drifting_solution.txt|drifting.taf|ketigid|SCR_SKIP_WAITKEY=1
# Gotcha (AIF). Campus water-gun. Hairpin, sprinklers, towel, kiss, squirt.
gotcha_solution.txt|gotcha.taf|dinner and a movie buster|SCR_SKIP_WAITKEY=1
# The Sorcerer. Job raise, master staff, ogre, ship, freeze guard, king, staff, jump.
thesorc_solution.txt|thesorc.taf|Congratulations-You've Done It!|SCR_SKIP_WAITKEY=1
# A Friendly Party (AIF). Greet, pizza, beer, kings, Denise, peek, sleep.
afp_solution.txt|AFP.taf|resting your head against the cushions|SCR_SKIP_WAITKEY=1
# cellpart1 teaser. Wait for door, Genevive handshake, cabinet.
cellpart1_solution.txt|cellpart1.taf|gingerly step over the booby trap|SCR_SKIP_WAITKEY=1
pb_solution.txt|PB.taf|freed the girls|SCR_SKIP_WAITKEY=1
# When the Lights Go Out. Oil the trapdoor, cave, city of the lights.
lightsgoout_solution.txt|lightsgoout.taf|Welcome to the City of The Lights|SCR_SKIP_WAITKEY=1
# First Day of School. Dress, Frooties at the dinette, sidewalk to the hydrant.
firstday_solution.txt|firstday.taf|yellow fire hydrant|SCR_SKIP_WAITKEY=1
# The Average Life. Cupboard card, Baldy, refuse Joe's gun.
# 30/35 is the ceiling: task 5 `shoot` (+5) and task 6 `refuse` (+10) are the
# two room-7 endings; shoot is EndGame Var1=2 (death, 25/35), refuse the win.
average_solution.txt|average.taf|not-too-average past|SCR_SKIP_WAITKEY=1
# Time Machine (CLC). Glass, sandwich, 9217, gravity 3491, teleport 65197.
timemachine_solution.txt|timemachine.taf|aquaplums from a silver spoon|SCR_SKIP_WAITKEY=1
# Night and Day / Notebook2. Unpassworded notebook-input demo at the Red Cantina.
notebook2_solution.txt|Notebook2.taf|unpassworded|SCR_SKIP_WAITKEY=1
# Battlezone 3 Americans. Kill the tank, rush Nav 3, fighter, enemy recycler.
bz3americans_solution.txt|bz3_americans.taf|proud because of your wits and courage|SCR_SKIP_WAITKEY=1
# Battlezone 3 Soviets. Passes into town, shoot the prototype enemy recycler.
bz3soviets_solution.txt|bz3_soviets.taf|Good work comrade|SCR_SKIP_WAITKEY=1
# The Passages (3.9). Pontaco, Fontana box+sword, Dummy1-4, west to Outer World.
thepassages_solution.txt|thepassages.taf|breathtaking beauty of the Outer World|SCR_SKIP_WAITKEY=1
# The Fox (demo). Sorcerer, mage staff, troll, sorcerer village, east at the tower.
fox_solution.txt|fox.taf|You have now completed this part of The Fox|SCR_SKIP_WAITKEY=1
# Beethro's Text Adventure demo. Sword, stake in the fields, Dark Forest force field.
bta_solution.txt|bta.taf|It's only a demo.|SCR_SKIP_WAITKEY=1
athylon_solution.txt|athylon.taf|End of Game!!!|SCR_ASSUME_COMBAT=1 SCR_SKIP_WAITKEY=1
# Amnesia Kid. Styrofoam, banana tree, M-80 bookshelf, horsie, Tom's map, scientist pod.
amnesiakid_solution.txt|amnesiakid.taf|Some kind of strange pod|SCR_SKIP_WAITKEY=1
successor_solution.txt|The_Successor.taf|Godspeed on a life beyond your imagination!|SCR_SKIP_WAITKEY=1
# Wizards Playground. Elf/light, four remote heaven bolts, troll tooth to Rex.
wizards_solution.txt|Wizards_Playground.taf|You give him the troll tooth. "Very good. Here have some gold."|SCR_SKIP_WAITKEY=1
# Grand Journey demo. Fontana box/sword, cave dummies, hound, Carlos, guilded sword, Big Sister.
grandjourney_solution.txt|grandjourney.taf|You have finished the DEMO version of|SCR_SKIP_WAITKEY=1
# Sk8 Sponsorz. 998 park kickflips to Birdhouse (turn-timed events).
sk8sponsorz_solution.txt|sk8sponsorz.taf|You succeed and join Birdhouse.|SCR_SKIP_WAITKEY=1
# G7056. Cake/fuse, jump, garage, generator, captor, 10x sniper sentry, roof beacon.
g7056_solution.txt|g7056.taf|MISSION COMPLETED!!!|SCR_SKIP_WAITKEY=1
# Space Run demo. Hand/scanner, keycard, fuse wires, stairs button (Adam kills
# the stairs alien), customs, shower lever, turbo lift sector C.  290/290.
spacerun_solution.txt|spacerun.taf|wait for the full version to find out|SCR_ASSUME_ROOMS=1 SCR_SKIP_WAITKEY=1
# Enigma. Corridor fight, copper/brass keys in the oak doors, authored warlord death, stone door.
enigma_solution.txt|enigma.taf|clawed feet scrape across the mountain rocks|SCR_ASSUME_COMBAT=1 SCR_SKIP_WAITKEY=1
# Seek and Enjoy. Chores then train; go down not d (toys death); drop card before toilet doorbell.
seekandenjoy_solution.txt|seekandenjoybybackmasker.taf|some time to yourself|SCR_SKIP_WAITKEY=1
# Ghoster. Occupy alien, stand on hill, beacon, ferry body, crowbar bulkhead, occupy captain, kill robot.
ghoster_solution.txt|ghoster.taf|But it is at least a victory.|SCR_SKIP_WAITKEY=1
# Noximion. Shield, Barrens port go to 1 (event), b, 16 buzzards, wait for dungeon complete.
noximion_solution.txt|noximion.taf|defeated all monsters and completed the dungeon|SCR_SKIP_WAITKEY=1
# Hunting Ground. Greet, pistol/dagger/pearls, dump corpses, leave John in blue, Libby last, jump dock.
huntingground_solution.txt|Hunting Ground.taf|too much upon his pistol|SCR_SKIP_WAITKEY=1
# Go. House, small-door engraving, diary teleport, cash machine, office, taxi. 190/190.
go_solution.txt|Go.taf|adventures are behind|SCR_SKIP_WAITKEY=1
# ??????? ???????? demo. Stove basket, pies, hat in the apple tree, out the gate.
demoshapka_solution.txt|DemoShapka.taf|66%|SCR_SKIP_WAITKEY=1
# ?????. Feed the dog, eat the apple, move the weight, take the key, leave.
# Win marker on the proba row is cp1251, matching the transcript.
proba_solution.txt|proba.taf|вышли из этой квартиры|SCR_SKIP_WAITKEY=1
# Akron, Russian. Scanner, stake, dry the pass on the wasteland stone, tuxedo and ring.
akron_rus_solution.txt|akron_rus.taf|78%|SCR_SKIP_WAITKEY=1
# Nightmare on Elm Street (Russian). Valve, coffee, alarm, go to work.
elmstreet_solution.txt|A Nightmare on Elm Street.taf|100%|SCR_SKIP_WAITKEY=1
crimescene_solution.txt|CrimeScene.taf|Trent smashed the window|SCR_SKIP_WAITKEY=1
crimescene_patched_solution.txt|CrimeScene.taf|You scored 80 out of the maximum 80!|SCR_ASSUME_PATCHES=1 SCR_SKIP_WAITKEY=1
schoolday_solution.txt|SchoolDay.taf|YOU WIN!!!!!|SCR_SKIP_WAITKEY=1
sororityHouse_solution.txt|sororityHouse.taf|Congratulations, you win!|SCR_SKIP_WAITKEY=1
freshman_solution.txt|freshman.taf|Five times.  That last time was a doozy!|SCR_SKIP_WAITKEY=1
prostitute_solution.txt|prostitute.taf|Congradulations, you did it!|SCR_SKIP_WAITKEY=1
lovingfamily_solution.txt|Loving Family.taf|Again Congrats and thanks for playing|SCR_SKIP_WAITKEY=1
fantasy_solution.txt|fantasy.taf|I love working with you Player, maybe we can do some buisniss together soon|SCR_SKIP_WAITKEY=1
loveforreal_solution.txt|LoveForReal.taf|your ship safely passes along them|SCR_SKIP_WAITKEY=1
villagelove_solution.txt|The Village of Love and Lust.taf|overworked yourself and fainted|SCR_SKIP_WAITKEY=1
mwf_solution.txt|mwf.taf|Time passes...|SCR_SKIP_WAITKEY=1
superstud_solution.txt|The new Superstud.taf|you win the prize which is me|SCR_SKIP_WAITKEY=1
bedlam_solution.txt|bedlam.taf|These type of vehicles usually require keys to operate|
bedlam_patched_solution.txt|bedlam.taf|You have just completed the Bedlam preview|SCR_ASSUME_PATCHES=1
crimelife_solution.txt|crimelife.taf|You just got waxed by a punk gangsta|
dbaa_solution.txt|dbaa!(intro).taf|Bring me something else!|
dickynoodle_solution.txt|DickyNoodle.TAF|You untie your loving Uncle Noodle|
dishduty_solution.txt|dishduty_intro(3).taf|You can't wash that.|
illegalsocks_solution.txt|illegalsocks.taf|Your score is 745 out of a maximum of 2155.|
illegalsocks_patched_solution.txt|illegalsocks.taf|The Great Doctor manages to avoid your attack with Awesome Sword|SCR_ASSUME_PATCHES=1
ebonysworld_solution.txt|ebonysworld.taf|Your score is 1450 out of a maximum of 0.|
ebonysworld_patched_solution.txt|ebonysworld.taf|the colony is saved|SCR_ASSUME_PATCHES=1
liqid_solution.txt|liqid.taf|I surrender, you win!|
liqid_patched_solution.txt|liqid.taf|You scored 95 out of the maximum 100!|SCR_ASSUME_PATCHES=1
mages_solution.txt|mages.taf|a magic rating of 20, and your mana=50.|
monsterisland_solution.txt|MonsterIsland.taf|Ameila and I rushed to her plane and we flew off to safety.|
mysteryhouse_solution.txt|MysteryHouse.taf|You drop the Treasure Chest.|
# The same route with the engine's targeted game patches on: "open chest" gets
# the action it was missing, so the chest really is open when the route drops
# it, and the game prints its WINTEXT.
mysteryhouse_patched_solution.txt|MysteryHouse.taf|You got out of this world!|SCR_ASSUME_PATCHES=1
newbie_solution.txt|newbie.taf|You go throo the wall|
hotelconfuego_solution.txt|1_Hotel_con_Fuego.taf|Well, that's the end of the demo.|
teaw_solution.txt|TEAW_(introcomp).taf|YOU ARE GOING TO DIE|
cloddemo_solution.txt|clod_demo.taf|You scored 140 out of the maximum 140!|
night_solution.txt|The_Night_That_Dripped_Blood.taf|You scored 100 out of the maximum 100!|
thewill_solution.txt|The_Will.taf|You have completed The Will and inherited a fortune.|
# Twenty-one.taf: horror chase vignette. Full WIN -- silver flask, glasses
# and cane are mandatory survival gear; escape corridor then let the
# countdown expire. See notes/TwentyOne_walkthrough.md.
twentyone_solution.txt|Twenty-one.taf|survived long enough to get the best ending|
# weirdstuff2.taf: horror opener. Best reachable, not a win -- WINTEXT is
# empty; entering room 1436 is an unavoidable trap into a Cell with zero
# EXIT entries (genuine engine dead end). See notes/Weirdstuff2_walkthrough.md.
weirdstuff2_solution.txt|weirdstuff2.taf||
filthybill_solution.txt|filthybill.taf|I appreciate your help with Dave|
filthybill_patched_solution.txt|filthybill.taf|Well done - you scored maximum points!|SCR_ASSUME_PATCHES=1
temporfell_solution.txt|temporfell_demo.taf|Thanks for testing|SCR_RNG=xoshiro
thenightmoon_solution.txt|thenightmoon.taf|You scored 360 out of the maximum 400!|SCR_RNG=xoshiro
thenightmoon_patched_solution.txt|thenightmoon.taf|You scored 380 out of the maximum 400!|SCR_RNG=xoshiro SCR_ASSUME_PATCHES=1
# 2026-09-25 batch: derived walkthroughs for content-clean unwired games
# (notes/<Game>_walkthrough.md for each). zanoza: WIN 28/29, marker is the
# cp1251 task-91 win text.
zanoza_solution.txt|zanoza.taf|Поздравляю с победой|SCR_SKIP_WAITKEY=1
# Storm Tossed: WIN 305/305 (EndGame WINTEXT).
tempest7_solution.txt|tempest7.taf|Congratulations, you have won!|SCR_RNG=xoshiro
# Imagings: demo, no score/ending; deepest point Church Road.
imagings_solution.txt|imagings.taf||
wonderland_solution.txt|wonderland.taf|The Tempest has put you someplace different|SCR_ASSUME_COMBAT=1 SCR_ASSUME_CAPACITY=1 SCR_SKIP_WAITKEY=1
# Short-lived: unwinnable by design, MaxScore 0.
shortlived_solution.txt|shortlived.taf||
# The Monster in the Mirror: WIN 100/100.
monstermirror_solution.txt|monster.taf|So you figured it out|
# The Annihilation of Think.com 3: 0/1, the only win (task 22) is unreachable.
taot3_solution.txt|TAOT3.taf||
taot3_patched_solution.txt|TAOT3.taf|Well done - you scored maximum points!|SCR_ASSUME_PATCHES=1
# Last Knight: abandoned opening, no tasks; answers the name prompt only.
lastknight_solution.txt|Last_Knight.taf||
# Govard. Zabvenie part 2 (3.90, Russian): WIN 22/22. Lion/gargoyle/bear fights
# depend on the xoshiro stream (enemy picks player or Romin; Romin's death ends it);
# the two waits before the bear matter -- re-derive if anything earlier changes.
govard2_solution.txt|Govard2.taf|готов к решающей битве|
dolg_solution.txt|Dolg.taf|я и расплатился с Барни|
govard_solution.txt|Govard.taf|На этом первая часть приключений|
# Место преступления 2 (3.90, Russian): best reachable 23/70, no win.
cs2_solution.txt|CS2.taf||
cs2_patched_solution.txt|CS2.taf|You scored 70 out of the mximum 70!|SCR_ASSUME_PATCHES=1
# Шаблон детектива (3.90, Russian): author sandbox, coverage walk, no scoring.
shablon_solution.txt|shablon.taf||
# Странники: врата миров 0.04 (3.90, Russian): end of demo, 29/29, no type-6 win.
wanderersgow_solution.txt|WanderersGoW 0.04.taf||
# NAT_01 (Nathaniel Peck, case 1; 3.90, Russian): WIN 18/18. Line 1 is blank for
# the intro waitkey.
nat01_solution.txt|NAT_01.taf|Вы прирожденный детектив|
# ReLife v1.14 (3.90, Russian): WIN 400/400. Line 1 answers the name prompt; 6 waits
# before arming the bomb (safe window 3-12 under xoshiro), then exactly 9 moves out.
relife_solution.txt|Relife.taf|Это естественный человек|
# The World According To CBN: Clueless Bob spin-off, no scoring. WIN = TASK 111
# moves you to the <victory> room; class answers need the waitkey skip.
worldcbn_solution.txt|The_World_According_to_CBN.taf|a well-earned pat on the back|SCR_SKIP_WAITKEY=1
# How Did I Get Into This?: all four endings are losses -- no win exists; the
# route takes the story ending (TASK 15). Blank line 1 answers the title waitkey.
hdigit1_solution.txt|hdigit1.taf||
# A Day In Toronto (3.90): sandbox, no score/events/ending -- tour of all 20 rooms.
toronto_solution.txt|toronto.taf||
# The View Is Better Here: WIN. Two blanks after `get in car` (drive waitkey +
# a real turn); with one, `buy coke` is eaten.
viewbetter_solution.txt|the_view_is_better_here.taf|YOUR TASK IS COMPLETED. YOU HAVE...WON?|
# The Virtual Human: one-word questionnaire, WIN (TASK 127); each of the 12 blanks
# answers a [Press any key] waitkey.
virtual_solution.txt|virtual.taf|to understand in a few days.|
# Hammurabi: seed-locked route under SCR_RNG=xoshiro; wins at year 10, rating 231
# (best final-year choice for this route, not a proven maximum).
hammurabi_solution.txt|hammurabi.taf|Congratulations, Hammurabi!|
# Space Mercenary v0.1: menu demo, no score, empty WINTEXT, all 27 EndGames are
# failures; route ends at the unhandled Lork audience menu. RNG-tuned.
smercenary_solution.txt|smercenary.taf||SCR_RNG=xoshiro

# The GameMaster: Resident Lust: WIN, 45/56 (80%) -- the true ceiling, not the
# declared 56, which double-counts two mutually exclusive branches (see the
# solution file header). AIF, adults throughout.
vgm1_3_solution.txt|VGM1_3.taf|You scored 45 out of the maximum 56!|
# Ghost Justice (ghostjustice.taf), Purple Dragon: WIN, 100/100, in 182
# commands. Kill-not-torture branch for Margaret (proven 0-point either
# way). Deterministic across 3 runs. See goldens/ghostjustice_solution.txt.
ghostjustice_solution.txt|ghostjustice.taf|You scored 100 out of the maximum 100!|
# Blue Sky (bluesky.taf), v. 0.5: WIN, in 74 commands. Declared max score
# is 0 (no `ACT type=4` anywhere); win marker is the game's WINTEXT header.
# Deterministic across 3 runs. See notes/bluesky_walkthrough.md.
bluesky_solution.txt|bluesky.taf|You have finished Blue Sky (v. 0.5)|
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
#
# SCR_ECHO_INPUT=1 makes os_ansi echo each command after its '>' prompt, as
# "\n> command\n" -- the same shape a5run_dump gives the ADRIFT 5 goldens.
# Without it the goldens record only the replies, so reading one means counting
# prompts against the solution file by hand, and a route that desyncs by one
# command is invisible in the diff.
#
# V4WT_STDERR_DIR, if set, keeps each row's stderr there as
# <game>.<solution>.err (trace_deviations.sh reads it).
transcript() {  # $1=game path $2=solution path
  { cat "$2"; echo quit; echo y; } \
    | ( ulimit -t 30; env SCR_ECHO_INPUT=1 SCR_RNG=xoshiro $ROW_ENV "$SCARE_BIN" "$1" \
        2>"${V4WT_STDERR_DIR:-/dev/null}${V4WT_STDERR_DIR:+/$(basename "$1").$(basename "$2").err}" ) \
    | tr -d '\r' | sed 's/[[:space:]]*$//' | cat -s
}

# Build the harness if it's missing OR older than any engine source.  The
# missing-only check once let a whole corpus run "pass" against a stale binary
# (the wield-model port, 2026-08-01) -- never again.  os_ansi.cpp is in the set
# too: it is the port that prints the transcript (prompt, echo, line wrap), so
# editing it changes every golden while matching none of the sc*.cpp globs.
# library/ and runner/ are searched too: the find once stopped at the top level
# and missed every edit to the command library and the runner.
# (Makefile.headless `test` builds $SCARE_BIN from its shared objects before it
# gets here, so this is the standalone path.)
SRC_DIR="${SCARE_DIR:-$(cd "$HERE/../../.." && pwd)}"
if [ ! -x "$SCARE_BIN" ] \
   || [ -n "$(find "$SRC_DIR" "$SRC_DIR/library" "$SRC_DIR/runner" -maxdepth 1 \
              \( -name 'sc*.cpp' -o -name 'os_ansi.cpp' -o -name 'mapdraw.cpp' \
                 -o -name '*.cpp' -path '*/library/*' -o -name '*.cpp' -path '*/runner/*' \
                 -o -name '*.h' \) \
              -newer "$SCARE_BIN" 2>/dev/null | head -1)" ]; then
  echo "building headless scare harness (build.sh)..." >&2
  SCARE_DIR="${SCARE_DIR:-}" sh "$HERE/build.sh" >&2 || {
    echo "run_v4_walkthroughs: build failed" >&2; exit 2; }
fi

WORKDIR=$(mktemp -d); trap 'rm -rf "$WORKDIR"' EXIT

# One MAP row, start to finish.  Writes its table line to $WORKDIR/<idx>.row
# and, if it regressed, its solution name to $WORKDIR/<idx>.reg.  Runs in the
# background (up to $JOBS at once); the rows are printed in MAP order once
# every job has finished, so the table looks the same as it did when this ran
# serially.
run_one() {  # $1=idx $2=sol $3=game $4=marker $5=envs
  idx=$1 sol=$2 game=$3 marker=$4
  row="$WORKDIR/$idx.row"
  reg="$WORKDIR/$idx.reg"
  ROW_ENV=$5                                  # read by transcript()
  solpath="$HERE/../goldens/$sol"
  golden="$HERE/../goldens/${sol%.txt}.expected.txt"

  [ -f "$solpath" ] || {
    printf "%-34s %-9s\n" "$sol" "NOSCRIPT" > "$row"; return; }
  gp=$(find_game "$game")
  [ -n "$gp" ] || {
    printf "%-34s %-9s (%s)\n" "$sol" "SKIP" "$game" > "$row"; return; }

  out=$(transcript "$gp" "$solpath")

  # Optional win-marker guard.
  markok=1
  if [ -n "$marker" ]; then
    printf '%s\n' "$out" | grep -Fq "$marker" || markok=0
  fi

  if [ "$BLESS" = 1 ]; then
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (win marker '%s' absent -- NOT blessed)\n" \
             "$sol" "REFUSED" "$marker" > "$row"
      echo "$sol" > "$reg"
    else
      printf '%s\n' "$out" > "$golden"
      printf "%-34s %-9s -> %s\n" "$sol" "BLESSED" "$(basename "$golden")" > "$row"
    fi
    return
  fi

  if [ ! -f "$golden" ]; then
    # No golden yet: not a hard failure, but flag it, and fail if a declared
    # win marker is missing (a losing transcript must never look "ok").
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (no golden AND win marker '%s' absent)\n" \
             "$sol" "FAIL" "$marker" > "$row"
      echo "$sol" > "$reg"
    else
      printf "%-34s %-9s (run --bless to record)\n" "$sol" "NEEDGOLD" > "$row"
    fi
    return
  fi

  if printf '%s\n' "$out" | diff -q "$golden" - >/dev/null 2>&1 && [ "$markok" = 1 ]; then
    printf "%-34s %-9s\n" "$sol" "PASS" > "$row"
  else
    if [ "$markok" = 0 ]; then
      printf "%-34s %-9s (win marker '%s' absent)\n" "$sol" "FAIL" "$marker" > "$row"
    else
      printf "%-34s %-9s (golden mismatch)\n" "$sol" "FAIL" > "$row"
    fi
    [ "$VERBOSE" = 1 ] && printf '%s\n' "$out" | diff "$golden" - | sed 's/^/    /' >> "$row"
    echo "$sol" > "$reg"
  fi
}

printf "%-34s %-9s %s\n" "SOLUTION" "STATUS" "detail"
printf "%-34s %-9s %s\n" "--------" "------" "------"

# Fan the rows out $JOBS at a time.  Throttle by keeping $JOBS RUNNING jobs
# rather than launching $JOBS and waiting for all of them: row runtimes are
# skewed (the biggest ALR/turn-count games are several times the median), so a
# batch barrier costs the slowest member of every batch.  `jobs -pr` (running
# only) is the portable-enough probe -- /bin/sh here is bash 3.2, which has no
# `wait -n`.
#
# The subshell created by the `map_rows |` pipeline owns the jobs, so the
# trailing `wait` has to stay inside the braces.
map_rows | {
  n=0
  while IFS='|' read -r sol game marker envs; do
    [ -z "$sol" ] && continue
    case "$sol" in '#'*) continue ;; esac       # comment row
    case "$sol" in *"$FILTER"*) : ;; *) continue ;; esac
    n=$((n+1))
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do sleep 0.05; done
    # </dev/null: a child inherits THIS while-loop's stdin (the map_rows pipe).
    # transcript() feeds the solution file to `scare` on a pipe of its own, but
    # anything here that read stdin would silently EAT map rows -- the row-count
    # backstop below is what would catch that.
    run_one "$(printf '%04d' "$n")" "$sol" "$game" "$marker" "$envs" </dev/null &
  done
  wait
  echo "$n" > "$WORKDIR/expected_rows"
}

for row in "$WORKDIR"/*.row; do
  [ -f "$row" ] && cat "$row"
done

echo
echo "PASS = transcript matches golden (+ win marker if set); NEEDGOLD = derived"
echo "but not yet recorded (run --bless); SKIP = game .taf absent; NOSCRIPT = no"
echo "solution file; FAIL = golden mismatch or missing win marker."

# Backstop: every selected MAP row must have produced a .row file.  A shortfall
# means rows were lost, and a truncated table otherwise reads as "everything ran
# and passed".
expected=$(cat "$WORKDIR/expected_rows" 2>/dev/null || echo 0)
actual=$(ls "$WORKDIR"/*.row 2>/dev/null | wc -l | tr -d ' ')
if [ "$actual" -ne "$expected" ]; then
  echo
  echo "ERROR: only $actual of $expected MAP rows ran -- this run is INCOMPLETE" >&2
  exit 1
fi

REGRESSIONS=$(cat "$WORKDIR"/*.reg 2>/dev/null | tr '\n' ' ')
if [ -n "$REGRESSIONS" ]; then
  echo; echo "REGRESSIONS: $REGRESSIONS"
  exit 1
fi
exit 0
