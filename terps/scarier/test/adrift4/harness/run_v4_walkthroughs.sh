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
circus_solution.txt|circus.taf|Congratulations.  You completed the game|SCR_SEED=12
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
inverness_solution.txt|inverness.taf|Your score is 75 out of a maximum of 205.
les_feux_solution.txt|Les Feux de l'enfer.taf|Votre score est 25 sur un maximum de 115.|SCR_SEED=138
lifesimulation_solution.txt|lifesimulation.taf|Your score is 0 out of a maximum of 0.
matts_house_solution.txt|Matt's House.taf|Your score is 5 out of a maximum of 5.
mr_smith_solution.txt|The_Search_For_Mr_Smith.taf|Congratulations! I hope you liked our game.
phoenix_destiny_solution.txt|Phoenix_Destiny.taf|Gold: 100
questi_solution.txt|QuestI.taf|Your score is 10 out of a maximum of 10.
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
# ...and the *max* assisted row, 265/373 (the row above banks 265-17=248).  The
# extra 20 comes from task 72 `^^aquired armor^^` (Theeve's death reward), which
# NOTHING in the game executes -- To Hell & Beyond is an upgraded 3.9 file and
# 3.9 has no execute-task action at all, so every chain runs through events /
# NPC walks / battle KilledTask, and Theeve (NPC 28, a fully configured hostile)
# was left with killedTask=-1.  The only way to fire it is to walk to room 128
# and type the author's internal task name, so this row is an EXPLOIT row, not
# an honest maximum -- keep the 248 row above as the honest assisted result.
# The 20-move round trip costs one -3 from the ^^Days^^ timer (unavoidable:
# trimming the trailing waits 18->4 still wins but does not dodge it), hence
# +17 net.  The remaining gap to the 293 ceiling is task 83 `greet Trace` (+25),
# which is unreachable: an NPC walk's charTask fires task 89 `^^discussion^^` in
# room 166 and teleports the player out on the very turn they enter.  373 itself
# is NOT the ceiling -- tasks 86 `go home` (+80) and 87 `claim the throne`
# (+150) both carry an ACT type=6, so only one of the two can ever be banked.
to_hell_and_beyond_assisted_max_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1
# Villains_And_Kings is a V390 file, so battle_legacy skips the acc>agi gate and
# the assassin is killable with no aid at all -- the old 13/37 "faithful" row and
# its SCR_ASSUME_COMBAT=1 companion (30/37) both rested on 4.0 combat rules being
# applied to a 3.9 game, and the assisted row is retired.  31/37 is the true
# maximum: task 5 (`take soap`, +1) has Where=NO_ROOMS so it can never run, and
# tasks 2/17 (`give soap`/`yes`, +5 each) are duplicates that consume the one
# soap.  SCR_ASSUME_COMBAT still has a row above (to_hell_and_beyond, a real 4.0
# zero-accuracy game).
villains_and_kings_solution.txt|Villains_And_Kings.taf|Your score is 31 out of a maximum of 37.
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
cursed_solution.txt|cursed.taf|The honour will be all mine, father|SCR_SKIP_WAITKEY=1
easter_solution.txt|easter.taf|***You have won***|
yonastoundingcastle_solution.txt|yonastoundingcastle.taf|Incredible victory!|SCR_SKIP_WAITKEY=1
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
# The `downloaded/` sweep (2026-08-03): games whose upstream walkthrough was
# harvested off IFDB into test/adrift4/downloaded/ and whose .taf was
# already on this machine.  See downloaded/INDEX.md for the provenance of each
# walkthrough and WALKTHROUGH_TODO.md for the per-game notes.
ptbad_solution.txt|ptbad.taf|You Win! Yay!
# The four Richard Otter games replay command-for-command off delron.org.uk's
# own walkthrus; each needs a leading "1" for the game's title menu (the
# published lists start at the first in-game command) and SCR_SKIP_WAITKEY for
# the "[Press ENTER to continue]" splash.
vague_solution.txt|vague.taf|You have won!|SCR_SKIP_WAITKEY=1
escape_to_new_york_solution.txt|EscapeToNewYork.taf|You managed to score 100 out of 100 and completed all of your objectives.|SCR_SKIP_WAITKEY=1
unauthorized_termination_solution.txt|unauthorized.taf|Assignment Status: You have been successful.|SCR_SKIP_WAITKEY=1
# Where Are My Keys? needed one derivation step the shipped walkthru omits: it
# says "(now you need to find the dog)" and leaves it at that.  The dog (NPC 2)
# starts in the Back Bedroom and wanders, so the bone is handed over there, not
# in the kitchen -- then it trots to the back door on its own and `open door`
# (task 294 -> 293) lets it out to bury the bone in the Vegetable Patch, which
# is what unearths the car keys (task 292).
where_are_my_keys_solution.txt|WhereAreMyKeys.taf|You start the car and head home.|SCR_SKIP_WAITKEY=1
# To Hell in a Hamper: the IF-Archive walkthrough desyncs badly on this release.
# It has to be re-derived around a carry-weight limit ("too heavy for me to carry
# at the moment"), so the trombone/mallet/scissors/smudge stick all go overboard
# BEFORE `get hatchet`; `cut anchor rope` alone is rejected, it must be
# `cut anchor rope with hatchet`.  Throwing the mummy triggers THE CURSE OF THE
# BLUE IBIS -- which is also what makes Hubert produce Aunt Gertie from his
# overcoat (she does not exist before that) -- and the ibis kills you ~3 turns
# later unless the smudge stick (an eagle returns it after you throw it) is
# relit with the lighter from the inside pocket.  Finally `push gertie` grants
# exactly ONE turn, and it must be `pull gas valve rope`; the boomerang is a
# trap that returns and knocks you out.
to_hell_in_a_hamper_solution.txt|Hamper.taf|reached the incredible altitude of 37,000 feet|SCR_SKIP_WAITKEY=1
# Lost has no score and two mutually exclusive endings, so both are wired.  The
# shipped walkthru's endgame ("z / g / g / g / follow ghost / g / talk to ghost
# / g / g / g / down or up") is off by a turn: the phantom event (EVENT 5,
# every 5 turns) has to have fired at least once before `follow ghost` does
# anything but "There is no phantom here", and `talk to ghost` only parses in
# the Rocky outcropping (task 28, where=room 17) -- so it is five waits, two
# follows, then `x ghost` and exactly four talks before the ghost dissolves.
lost_solution.txt|LOST.TAF|place your foot on the path leading up the crumbling cliff
lost_down_solution.txt|LOST.TAF|has shown you a doorway back to that brighter world.
# Marika the Offering is a one-room siege: the walkthru is prose ("there are
# five ways for The Count to get into the room") and names no commands, so the
# route is derived from the task table -- window shut+locked with the hairpin,
# fire lit in the fireplace, blanket under the door, rag in the keyhole and the
# flagstone put back -- then 33 turns of `z` to survive the night.  Losing the
# crucifix down the fissure when you reach into the hole is scripted, not a
# desync.
marika_solution.txt|marika.taf|I plan to enjoy every second of it.|SCR_SKIP_WAITKEY=1
# Vendetta's walkthru stops at "(Final Scene : How you win is up to you.)", and
# it silently assumes you burn a command on each of the game's two in-text
# "*Press Enter*" prompts (SCR_SKIP_WAITKEY doesn't cover those -- they are
# ordinary command turns), so the script carries a blank line at the start and
# another before `x door`.  Its second `enter` in the hotel-lounge cutscene is
# also a no-op; one more `wait` there gets you to the Press Enter.  The final
# scene is an eight-round numbered-choice duel with the griffon: the winning
# line without any of the optional weapons is `kill griffon` then 3 / 2 / 1 / 2
# (round 1 dodge-and-counter is exactly the lesson from the opening dream).
vendetta_solution.txt|Vendetta.taf|The End|SCR_SKIP_WAITKEY=1
# Unraveling God is puzzleless and ends on a two-way choice ("Pressing either 1
# or 2 will be the end of the story, but the results are very different"), so
# both endings are wired.  The author's own walkthru interleaves bracketed
# commentary with the commands and never spells out the house sequence: `skip
# delay` has to come BEFORE `answer the door` (it is what rings the bell), and
# the "call 911" line is a parenthetical list of synonyms, not four commands.
unraveling_god_solution.txt|unravel.taf|smile as the river burns through your flesh.|SCR_SKIP_WAITKEY=1
unraveling_god_lou_solution.txt|unravel.taf|smile fades and you feel the beginnings of fear.|SCR_SKIP_WAITKEY=1
# My Mind's Mishmash replays its shipped walkthru command-for-command bar one
# turn: the Episode 3 note "Wait for the laser gun to point at you" collapses an
# unknown number of waits into a single `z`, and on this release the alien
# ship's laser only swings round on the SECOND wait -- so it is `rc`, `z`, `z`,
# `wc` (put the cap back on and the laser blows the mine door instead of you).
mishmash_solution.txt|mishmash.taf|You have lived up to your name and survived again!|SCR_SKIP_WAITKEY=1
# The Hangover is UNWINNABLE as shipped, and the two dead ends are the author's,
# not ours.  Both of the tasks the walkthrough's endgame turns on carry
# Where/Type = 0 (ROOMLIST_NO_ROOMS), so they can never run in ANY room:
#
#   TASK 10 where=0 [give the doctor some french fries]   -- 2nd approval form
#   TASK 14 where=0 [give approval notes to platypus]     -- the winning ending
#
# Every other room-scoped task in the game is where=1.  Confirmed against the
# real run390.exe, which answers "You can't do that here!" to both -- so 5 out
# of 7 is the ceiling (bill->fries, kick bum, mail->secretary, open the filing
# cabinet, type the toothbrush code).  Losing the doctor costs only the point:
# the Psycho Hospital's south exit is not gated on his keys, so the route runs
# to the end, and the Form Process Office even prints "You give approval notes
# to platypus" as scenery while the task itself sits unreachable.
#
# Two incidental notes: the room descriptions are unreliable (Fedrick Avenue
# says the bus stop is east; it is west), and `give the doctor some french
# fries` lands on the library's "Give what?" here where run390 prints its
# wrong-room message -- SCARE has no "You can't do that here!" at all.  See
# WALKTHROUGH_TODO.md for that divergence.
the_hangover_solution.txt|hangover.taf|Your score is 5 out of a maximum of 7.
# Troll! is WINNABLE and this route reaches the ending with zero parser errors,
# but its ceiling is 185/190, not 190.  The game has 38 scoring tasks worth 5
# each; TASK 82 can never be one of them:
#
#   TASK 80 [* pay * barman *]    needs obj67 "fourtune", turns it into obj68
#   TASK 82 [* pay * landlord *]  needs obj67 "fourtune", turns it into obj68
#   TASK 81 [* pay * landlord *]  needs obj68 "fortune",  turns it into obj69
#
# 80 and 82 consume the same object, so only one of them can ever fire, and 80
# is compulsory -- it is the task that summons the coach home (it moves obj21
# "coach" and obj78 "horse" and walks the player to room 3).  So the pair we
# take, 80 + 81, is the best available and 82's 5 points are dead.  The
# walkthrough never pays the landlord at all; the `in`/`pay landlord`/`out`
# detour at Outside Inn on the way home is ours.
#
# Three more points the walkthrough leaves on the floor, all recovered here:
#   * `take breadcrumbs` in the tavern -- TASK 85; the walkthrough says "get
#     crumbs", which the library happily resolves without firing the task.
#   * `w` in the upstairs corridor BEFORE unlocking the door -- TASK 86 is
#     gated on TASK 64 [* unlock * door *] NOT being done, so the order
#     matters: you score for walking into the locked door, then unlock it.
#   * `put breadcrumbs in basin` a SECOND time, after the firewater -- TASK 51
#     wants obj48, and obj48 only reaches the player's hands as an action of
#     TASK 52 (the firewater step).  The first put is the library's.
#
# Two command patterns are order-sensitive and the walkthrough has them the
# wrong way round: TASK 61 is [* show * barman * medallion *] with no reversed
# ALTCMD (so `show medallion to barman` does nothing) and TASK 83 is [* give *
# chief * backward burp berries *].  Finally, the walkthrough's opening
# `get all` overflows the carry-weight limit on a pink flyer, a green notice
# and a blue advert, so it is dropped and five explicit drops are added; the
# tavern drops must happen on ENTRY, not after `drink whiskey`, because a
# timed event throws the player outside three turns after the last drink.
#
# The win marker is the closing line rather than a score line: the game ends
# inside `tickle frog` and never prints a final score, so the `score` just
# before it reads 180 and the last task's 5 points land in the ending text.
troll_solution.txt|Troll.taf|clean by dinner time, I'll bust your head in!|SCR_SKIP_WAITKEY=1
# A Spot Of Bother wins at the author's own maximum, 100/100, and the upstream
# transcript needed exactly ONE repair in 270 commands: a second `push door` in
# the gymnasium.  The first push is a task that only reveals the trap wire
# ("A wire, previously hidden, slips into view"); it does not open the door, so
# after `break wire` the east exit still answers "You need to open the door
# first." and `open door` answers "You can't open the door!".  Pushing again
# opens it and the rest of the transcript replays verbatim.  The upstream file
# is a full session log including the title menu, so the first two commands are
# the menu picks `2` (read the introduction) and `1` (play).
spot_of_bother_solution.txt|A_Spot_of_Bother.taf|a grand total of 100 out of 100|SCR_SKIP_WAITKEY=1
# Beanstalk the and Jack (David Welbourn, 2008) replays the delron command list
# verbatim -- 49 commands, no repair at all, straight to "*** You have won ***".
# It is a reverse-chronology retelling, so the list reads backwards (it opens on
# `chop beanstalk` and ends with Jack waking up); that is the game, not a
# scrambled walkthrough.  No waitkeys: the transcript is byte-identical with and
# without SCR_SKIP_WAITKEY, so the row carries no env.
beanstalk_solution.txt|Beanstalk.taf|*** You have won ***
# Three more delron command lists that replay verbatim -- no repairs at all:
#   Black Sheep's Gold (Kent Tessman-style tall tale, 2004) -- 99 commands.  It
#     needs SCR_SKIP_WAITKEY: the epilogue stops on "(press any key to
#     continue)" and without it the pause eats the `quit`, so the ending never
#     prints.
#   Doomed Xycanthus (2006) -- 82 commands, ends on "Congratulations!".
#   Dancing Even Him? (Richard Otter, 2006) -- 17 commands; the title is an
#     anagram of "Vending Machine", which the ending text spells out, so that
#     line is the win marker.
black_sheeps_gold_solution.txt|BlackSheepsGold.taf|You've beaten Black Sheep's Gold!|SCR_SKIP_WAITKEY=1
doomed_xycanthus_solution.txt|xycanthus.taf|Then the gem flickers like a guttering candle and goes
dancing_even_him_solution.txt|dancingevenhim.taf|it is an anagram of Vending Machine
# The Demon Hunter (2003) -- WIN, 200/200, after two repairs to the delron list.
#   1. `south` -> `s`.  TASK 1 (where=1 room=6, the Armory) has cmd=[s] with
#      ALTCMDs [go s] [go south] [walk south] [wlk s] [walk s] -- the author
#      never listed the bare word `south`, so the walkthrough's `south` moved
#      the player normally without firing the task.  That task is the starter
#      task of EVENT 0 [monk's death] (StarterType=3, zero length, o2=3->10 =
#      move global object 2, the monk's prayer book, into room 7 The Chapel).
#      The book starts at pos=-1 (nowhere), so with `south` the event sat in
#      ES_AWAITING forever, the book never existed, and `get book` answered
#      "Take what?".  With `s` the task completes, the event starts and
#      finishes on the spot, and the book is in the Chapel to be taken.
#   2. `read book` added after `get book`.  TASK 2 (cmd=[read {it/the/a}
#      {monk's} {prayer} {book}], restricted to holding it) is worth 15 points
#      and the walkthrough never reads the book it just picked up.  Without it
#      the route tops out at 185/200.
# Hajar must be attacked EIGHT times, not the six the walkthrough lists: with 6
# or 7 kills the fight is unresolved, the score stops at 127 and the endgame
# never opens.  The 8th `kill hajar` scores 43 and the two `northeast` moves
# score 30 and end the game.  Needs SCR_SKIP_WAITKEY (the ending paginates).
# The closing line wraps, so the marker is only the part that stays on one
# line: "...calling to you<93>"Well done, my good and faithful" / "servant.""
the_demon_hunter_solution.txt|TheDemonHunter.taf|"Well done, my good and faithful|SCR_SKIP_WAITKEY=1
# Qui a tue Dana? (Volcy Bucherie / Christophe Montel) -- WIN, 100/100, the sum
# of every ACT type=4 in the game.  A French 4.0 game, so the solution file is
# stored in CP1252, NOT UTF-8: `prendre telephone` only parses when the accented
# e arrives as a single 0xE9 byte, and a UTF-8 file silently loses two objects.
# Four repairs to Hugo Labrande's upstream solution:
#   * A third bare `parler` at the crime scene.  There are three NPCs there and
#     three talk tasks; TASK 16 (chef scientifique) additionally needs TASK 18
#     (`soulever drap`) done first, so it can only fire after the sheet is
#     lifted -- which is why the upstream order leaves it unfired.  EXIT room=3
#     U is gated on TASK 16, so without it you are locked at the riverbank and
#     every remaining command fails.
#   * `u` -> `up`.  Two different things move you out of the crime scene: the
#     gated U exit, and TASK 19 cmd=[[up]].  `u` uses the exit only; the task
#     needs the literal word (the game's own synonyms are `haut` and `h`).
#     EXIT room=2 IN is gated on TASK 19, so after `u` the police station is
#     unreachable.
#   * `w` `w` inserted before the phone-memory presses and `e` `e` after.  TASK
#     24/25/26 (`appuyer 2` / `9` / `1`) are all where=1 room=4 -- your own
#     office -- and the upstream list presses them while standing in
#     MALKOWITCH's office, where they answer "Vous poussez Le 2, mais rien ne se
#     passe."  The upstream file even hedges here ("parfois ca ne marche pas");
#     the real reason is the room, not flakiness.
#   * `donner dossier` -> `donner malkowitch dossier`.  The winning task is
#     cmd=[[give] {malkowitch/...} {dossier}] and wants both words.
# Needs SCR_SKIP_WAITKEY (the epilogue paginates on "[Appuyer sur une touche
# pour continuer]").  Marker is the programmer's sign-off, the only pure-ASCII
# single line in the ending.
qui_a_tue_dana_solution.txt|QuiATueDana.taf|MERCI A TOI CHRISTOPHE SANS QUI CE JEU N'AURAIT JAMAIS VU LE JOUR!|SCR_SKIP_WAITKEY=1
# Enquete a hauts risques -- WIN, 59/59 ("Votre score est 58 sur un maximum de
# 59" one command before the end, and `se coucher` is the 59th point).  Another
# French 4.0 game, another CP1252 solution file.  A big one: 42 rooms, 165
# tasks, and the tasks are whole literal sentences with enormous ALTCMD lists
# (TASK 23 `prendre l'arme de service` carries 17 of them), so the upstream
# abbreviations mostly do parse.  Four repairs to Hugo Labrande's solution, all
# of them movement or timing:
#   * An extra `n` after arriving at the commissariat.  `e` from home parks you
#     at Devant le commissariat (room 10); the list then has one `n` where two
#     are needed (10 -> 11 L'accueil -> 12 Le couloir) before `w` reaches
#     Yannick's office.  Without it the next four commands all miss.
#   * The `s` after `rez-de-chaussee` deleted.  The lift already puts you in Le
#     couloir, and the stray `s` drops you to L'accueil, which has only N/S --
#     so the gun-cupboard detour (`e` to Votre bureau) fell off the map.
#   * Departure lounge: three `z` become four.  The boarding call is on a timed
#     event and lands on the fourth wait; the upstream `n` immediately after
#     three waits answers "Je dois attendre l'heure d'embarquement!".
#   * On board: two `z` become four.  EVENT 6 [Decollage] fires TASK 71 [d747]
#     -- the take-off -- and TASK 72 `regarder sous le siege` is restricted on
#     it, so looking under the seat before take-off finds nothing and the whole
#     bomb sequence (crate, wire cutters, three cables) never opens.
# No waitkeys: the transcript is byte-identical with and without
# SCR_SKIP_WAITKEY, so the row carries no env.
enquete_a_hauts_risques_solution.txt|EnqueteAHautsRisques.taf|Congratulations!
# Shadrick's Travels (Mystery) -- WIN, 100/100, and the whole game has exactly
# four scoring actions (20 + 20 + 10 + 50), all of which this route fires.  The
# upstream file is a session transcript with a CP1252 0xD8 as its prompt glyph,
# so the commands are the lines starting with that byte; 22 of them, replayed
# verbatim including the author's three duds (`x wood` and `climb tree` both
# hit the disambiguator, and `tire swing to tree` is a typo for `tie`).  They
# are kept because ADRIFT's "Please be more clear" does NOT consume the next
# line, so they cost nothing and the transcript stays faithful to the source.
shadricks_travels_solution.txt|ShadricksTravels.taf|Congratulations!
# Monsters (Release 2), Daniel Hiebert -- WIN, 40/40, which the SCR_DUMP_TASKS
# ACT type=4 total (40 over 8 tasks) confirms is the maximum; all 8 fire here.
# The upstream file is another real-Runner session transcript, command-then-
# response with no prompt glyph at all, so the 38 commands were lifted by hand.
# ONE repair: the author's `open the bedroom door` is answered "Open what?" --
# "bedroom door" collides with the *other* door object (obj13, the bedroom's
# own door, back in room 2), so it has to be the bare `open door` to reach
# obj48.  Everything else replays as published.
#
# This game paid for two genuine SCARE parser fixes, both with the author's own
# transcript as ground truth:
#   * uip_match_optional() did not rewind uip_posn when its look-ahead failed
#     *after consuming text*.  In "shine {the} [flashlight/light] {on} {the}
#     {brainsucker} {brain} {monster}" the look-ahead let {brain} eat the first
#     five letters of "brainsucker" (uip_match_word() is a prefix compare),
#     failed on "sucker", and the alternatives were then tried from there --
#     so `shine flashlight on the brainsucker` died and the game lost 5 points.
#   * %object% only ever matched "Prefix Short" or the bare Short, so
#     `examine the four poster bed` (Prefix "Sissy's four poster", Short "bed")
#     was "I see no such thing".  uip_build_candidate() now also answers to the
#     prefix with its leading words dropped.  Independently confirmed by
#     Shadrick's Travels, whose transcript shows `climb oak tree` answered
#     "You can't climb the old oak tree." -- SCARE used to say "that".
# Neither change moved any other golden in this suite.
monsters_solution.txt|Monsters_r2.taf|Congratulations!
# The Amulet (3-hour comp), Daniel Hiebert -- WIN, and a **verbatim** replay of
# the author's transcript: all 12 commands, no repairs, including the two pure
# flavour ones (`notes`, `spells`).  The game has NO scoring at all (`score`
# says "0 out of a maximum of 0", and SCR_DUMP_TASKS finds zero ACT type=4), so
# reaching the ending is the only measure there is.
#
# One deliberate difference from the published transcript: we print
# "Congratulations!" TWICE.  The winning task (TASK 3, #South - Win Game) ends
# its own CompleteText with that word, and the game's Header WinText is empty,
# so the engine adds its hard-coded default on top.  The Runner does the same
# -- Shadrick's Travels also has an empty WINTEXT and its winning task text
# does *not* contain the word, and its published transcript still shows one
# "Congratulations!", which is only possible if the Runner prints the default
# too.  So the author trimmed the duplicate when writing the walkthrough up;
# our transcript is the faithful one.  (`SCR_DUMP_TASKS` now prints a WINTEXT
# line, which is what settled this.)
the_amulet_solution.txt|TheAmulet.taf|Congratulations!
# Locked Door with Water Trap (KF Mini-Comp 2001).  Verbatim replay of the
# author's own session transcript, 21 commands, 1000/1000.  SCR_SKIP_WAITKEY=1
# is mandatory: the intro is three "Press any key to continue." screens, which
# would otherwise eat the first three commands.
locked_door_solution.txt|Locked_door_with_water_trap.taf|See if I ever dive with you two again|SCR_SKIP_WAITKEY=1
# Marooned is a TAF version 3.80 game, and the first one in this corpus that
# exercises the 3.8 size/weight conversion (see |V380_OBJECT:_SizeWeight_| in
# sctafpar.cpp) -- without it the dead seal and the tires are both "too heavy
# to carry" and the game cannot be finished at all.  80 of 140 is the ceiling:
# task 24 duplicates task 14's own "get trash" command and can never be the
# one that runs, task 27 needs the berries eaten but they are the monkey's
# price for the flint, task 35 wants the *unloaded* flare gun that loading it
# destroys (and firing the loaded one is fatal), and only the dented gas can's
# lighting task starts the Rescue event, so the scratched can's pour/light
# pair is mutually exclusive with winning.
marooned_solution.txt|marooned.taf|Congratulations, you are no longer Marooned!
# Wrecked (Campbell Wild, 2000), TAF 3.80.  WIN at the full 250/250, following
# the author's own published walkthrough -- but that walkthrough leaves four
# things to the reader that the harness has to spell out.  (1) Its bracketed
# "[wait for train...]" notes are real turns: the Ambersville train needs 2
# waits to pull in the first time and 10 the second, 7 more to reach Redstown
# and 5 to come back.  (2) Porkie the pig wanders after the first wave of the
# wand, so the second wave has to be repeated until he is actually in the room
# outside the Post Office.  (3) Two blocker tasks whose FailMessage the author
# left as the placeholder "x" swallow the plain commands "in" (the pub, once
# the scuba outfit is worn) and "up" (the Post Office roof, once you have
# climbed the statue); "go in" and "go up" miss those tasks' command lists and
# fall through to the room exit.  The pub one was checked live: run390 on a
# gen390 conversion of this file prints "x" and refuses entry exactly as we do,
# and "go in" works there too.  The roof one has the same shape and gen390
# re-encodes its restriction byte-identically to our parse.
# (4) "turn it" after "put key in ignition" binds to the ignition and hits the
# not-yet-started blocker; "turn key" is what scores.
wrecked_solution.txt|wrecked.taf|Hope you enjoyed playing Wrecked.
# Mortality (David Whyld, 2004).  A VERBATIM replay of the author's own session
# transcript shipped inside the game's doc file: all 78 commands, no repairs,
# word-for-word identical responses, ending on one of the two good endings.
# The game is scoreless (no ACT type=4 anywhere) and has no EndGame action at
# all (no ACT type=6), so reaching the ending text is the only measure -- the
# player is still standing afterwards, which is why the appended "quit" prompt
# shows up in the golden.  Needs SCR_SKIP_WAITKEY=1: the game is a
# menu-and-cutscene piece that paginates on "...press a key..." constantly.
# Crash it flushed out: task 314 [? the return] has a MoveObject action with
# Var1 = 2 ("the referenced object") but is only ever reached by redirection,
# so var_get_ref_object() hands back -1 and SCARE aborted on the range
# assertion in gs_object_make_hidden().  task_move_object() now ignores
# negative object indexes the way evt_move_object() already did.
mortality_solution.txt|mortality.taf|one of the two good endings|SCR_SKIP_WAITKEY=1
# Largo Winch (Jerome Marchand, 2005) -- WIN, 97/97.  A third French 3.90 game,
# so the solution file is CP1252 again.  323 commands, expanded from the
# author's own published list, which uses two conventions the interpreter can
# not take literally: "commande (N)" means repeat N times, and "combattre
# (terrasser l'ennemi)" means a whole fight has to be spelled out blow by blow.
# The four fights are the bulk of the repairs; each enemy answers to exactly one
# of "coup de poing"/"coup de pied" and the wrong one is usually instant death:
#   * Jack Place's flat: four bare `coup de poing`.
#   * Jack Place's courtyard: `coup de pied boris` x3 (kicking Boris is the only
#     line that lands every blow and takes no damage).  Only ONE of Boris and
#     Andre can be downed -- the other always flees -- so the list's "terrasser
#     les deux ennemis" overstates what the scene allows.
#   * Warehouse roof: kicks throughout, 2 + 2 at enemies 1 and 2 and a single
#     kick at enemy 3, which cues Simon to chain him.
#   * Hotel room 108: kick 1 x2, punch 2 x3, kick 3 x3.  The one-shot fire
#     extinguisher (a free double attack) is not needed and is left on the wall.
#   * Sharon's flat, Helena Dekovar: `coup de pied` x2.
# Four more repairs, all of them the published list being wrong or stale:
#   * `ouest` -> `nord` leaving the ground-floor corridor (its only exit is N).
#   * `est` -> `nord` into Sharon's salon (east is the kitchen).
#   * The Omega basement: giving Olga the devis already walks Largo downstairs,
#     so the extra `nord` is a blocked no-op, and the way back up is not `sud`
#     from where the bearded man sat -- the stairs are in a different room, "En
#     bas des escaliers", reached by stepping north into the corridor and back
#     south.  The coffee machine is then usable from the hall itself.
#   * `insérer la bague métallique dans l'armoire électrique` needs the word
#     `plate`: hammering the ring renames the object.
#   * `ouvrir la porte avec le badge` -> `utiliser le badge`, twice.  The game
#     defines an input synonym `ouvrir` -> `open` (SCR_TRACE_FLAGS=512 shows the
#     line rewritten to "open la porte avec le badge" BEFORE task matching), and
#     the hotel-door tasks 213/214/216 carry only "ouvrir ..." alt-commands with
#     no "open ..." twin -- unlike the window task 15, which has both.  So the
#     author's own phrasing can never fire; "utiliser le badge" is the same
#     task's primary command and the synonym leaves it alone.
# No waitkeys: the transcript is byte-identical with and without
# SCR_SKIP_WAITKEY, so the row carries no env.  The score reads 96/97 one
# command before the end and the last `salle du bigboard` is the 97th point.
largo_winch_solution.txt|largo-winch.taf|Congratulations!
# Three Monkeys, One Cage (Robert Goodwin, 2003) -- WIN, 98/100, and 98 is the
# ceiling: every one of the game's 23 scoring actions is banked.  The author
# wrote a `# jump out` chain whose +2 sits AFTER the two Execute-Task actions
# that end the game, and task_run_task_actions() stops at the first action that
# ends the game, so those last 2 points can never be displayed.  (Worth a
# run400 check some day -- see RUNNER_TESTS_TODO.md.)
# The route is built on the author's own prose solution in
# downloaded/ThreeMonkeysOneCage_solution.txt, but a lot of it had to be
# re-derived; the cage is a 2x2 room grid with two live monkeys walking it and
# a real-time fire, so ordering matters far more than the prose suggests:
#   * `quiet` first.  The author's running commentary is chatty and randomised;
#     turning it off is what makes the transcript stable.
#   * Do NOT pick the sheet up early.  `make fire` burns whatever tinder you
#     carry, and the sheet is worth only 3 fuel against the jersey's 5 -- and
#     the sheet is needed later, unburnt, as hornet armour.
#   * The mandrill kills on contact and you get exactly one action after it
#     shares your corner.  Two things fence it off: the fire permanently blocks
#     SW, and smoke blocks whichever corner the fan is aimed at (north -> NW,
#     east -> SE, northeast -> NE), which is why the fan is re-aimed four times.
#   * `cover myself with the sheet` fires task 637 (cover the *chimp*), which
#     shares that alt-command and wins on index.  Task 638's primary form,
#     `put the sheet over my head`, is the one that works.
#   * SW -> SE is `e`, not `se`: in a 2x2 grid the diagonal is a wall bump.
#   * Leaving SE silently unties the waist cord, so `tie cord to me` has to be
#     the last move before `jump out`.  The +4 is banked by the first tie.
#   * The 38 `z` in the middle are the game's design, not padding: the ceiling
#     panels open on turn 100.  Then `hide under bed` is mandatory -- the anvils
#     kill in five turns otherwise (they award +3 while you cower) -- and once
#     the anvils give way to bombs there are only nine turns to get out.
# The winnable oracle (task 21, 55 restrictions -- the corpus maximum) is the
# game that exposed the $RestrMask left-association bug fixed the same day; it
# now answers "The game is still winnable." from turn 1 to the jump.
3monkeys_solution.txt|3monkeys.taf|Congratulations, you did it!
# Humbug (Graham Cluley 1990/1997, converted to ADRIFT 4.00 by Campbell Wild)
# -- WIN with the FULL 2000/2000, "a winner.. or a cheat", in 1048 commands.
# The route is pjg's step-by-step solution for the ORIGINAL v5.0 game
# (downloaded/Humbug_walkthrough.sol), and the conversion turns out to track it
# so closely that all 125 of its annotated awards fire in order, with the same
# deltas and the same running totals, zero mismatches.  What the
# prose does NOT give you is turn counts, four hidden numbers, and one plural:
#   * The .sol writes "(keep looking until X shows up)" / "(wait about 35
#     moves)" -- every one of those had to become real turns.  The three that
#     matter are all-or-nothing: the bouncer waves me into the Golden Gulp only
#     while Grandad is beside me in the tunnel -- `S` after 9 `Look`s refused,
#     after 10 or 11 admitted, after 12 refused (the route uses 11);
#     the raffle package is handed over one turn AFTER the third-prize
#     announcement; and Horace gets his snuff tin out on a 10-turn cycle, so
#     the paper aeroplane has to be thrown on exactly that turn.
#   * `Get sheet` -> `Get sheets`.  Miss it and the tie-up of Dennis the
#     fireman silently fails; he wakes two rooms later and kills me.
#   * `Drop troch` is a typo for `Drop torch`.
#   * The combination door's buttons are a 7-segment display and the segments
#     do NOT reset when a digit is confirmed, so each digit is entered by
#     toggling the symmetric difference against the previous one; `Read
#     display` after each `push button 7` (the .sol's own advice) also keeps
#     Schrodinger the cat on the clock the mouse puzzle needs.
#   * Four placeholders, all read out of the game: the slate's MMMCDXLVI = the
#     dials 3-4-4-6; the aardvark scrawls HEL3761 for the keypad; the filofax's
#     green-ink "Viking Contact Society: 010473736401" and Olaf's National
#     Insurance number 60318897 (recited only once the balloon has cured his
#     hiccups) get the computer to display his aunty's 010473470651; and the
#     runes spell the magic word "Jisanajen".  Together they are worth 70 of
#     the 2000 -- without them the game still ends in a win, at 1930.
# Needs SCR_SKIP_WAITKEY=1: the "[Press any key]" title screen swallows the
# first two commands otherwise.
humbug_solution.txt|humbug.taf|Grandad would probably describe you as a winner.. or a cheat.|SCR_SKIP_WAITKEY=1
# Crime Adventure (M Whitmore) -- ADRIFT 3.80, 36 rooms, 23 tasks, 2 NPCs.
# WIN with the FULL 95/95 in 90 commands.  downloaded/CrimeAdventure_walkthrough.sol
# is a 29-line prose sketch by "sasi" that describes an EARLIER build: it wants
# you to read a computer in an "IBM" room for the stew recipe (there is none --
# the recipe is the cookery book in the kitchen), to dig a coin out of the
# ground with the shovel (the penny is in the spare-bedroom dresser) and to
# pick the underground door's lock with the hairpin (the door just opens).  The
# shovel, hairpin, fortune cookie, hat, picture, diary, painting, mirror and
# advertisement are all unused; the gypsy and the arcade-machine/street deaths
# are pure flavour.  What the route really needs, none of it in the .sol:
#   * TWO scored tasks are shadowed by unscored duplicates that sort first, so
#     each has to be issued TWICE.  `wear *shoes*` (task 14, 0 pts) shadows
#     `wear *golf* shoes` (task 15, 10 pts) -- so: wear, REMOVE, wear again.
#     `give *food* to mr fenwick` (task 12, 10 pts, alt `give *stew*...`)
#     shadows `give *stew* to mr fenwick` (task 17, 10 pts), and task 12's own
#     action drops the saucepan on the dining-room floor -- so: give, pick the
#     saucepan back up, give again.  Both pairs are needed for 95; a player who
#     types each command once tops out at 75.
#   * `get cash` in the arcade (task 19, 5 pts) prints "You grab the GBP30.00
#     from the machine" and does NOT move the object.  A second `get cash`
#     actually takes it -- and the cash is what task 15/16 check for.
#   * This is a 3.8 game, so the pooled burden model applies: limit 5, putter
#     costs 3, everything else 1.  Putter + ball + worn shoes is exactly 5, so
#     the cash has to be dropped in the kitchen and the ball has to be dropped
#     before the saucepan can be picked up for the second `give`.  Get this
#     wrong and the game answers "Your hands are full."
#   * 3.8 also only fills a dynamic container the player is HOLDING, so the
#     stew is loaded with the saucepan in hand (`get saucepan` first, and the
#     later one after `switch off cooker` is then redundant).  Cooking it does
#     not need the saucepan on the floor.  Measured in run380 on this very
#     game 2026-08-03: `put carrots in saucepan` with the saucepan where the
#     author left it answers "You are not holding a saucepan."
# No score is printed at the ending, so the route runs `score` (75/95) on the
# turn before the winning `stand on chair`, which banks the last 20.
crime_adventure_solution.txt|Crime_Adventure.taf|Mrs Fenwick was in no danger at all, it was a friend
# The Sisters (Andy Joel / "Mad Monk") -- ADRIFT 4.00, 50 rooms, 123 tasks,
# 9 events.  WIN with the FULL 109/109 in 151 commands.  All 109 points live in
# 38 `ACT type=4` add-score actions (every task's own score= field is 0), and
# the route fires all 38.  downloaded/TheSisters_walkthrough.txt is a good,
# honest 10-section prose guide that explicitly aims for 100%, so unusually
# little had to be re-derived -- but it does leave these gaps:
#   * `get tin` is "Take what?" -- the pickled herrings answer to `can`.
#   * `get key` in the music room is "You need to be more specific"; the object
#     is `iron key` (the guide's "large metal key" is the prose name only).
#   * On the lake, `row west` does not parse.  Plain compass movement works,
#     and `row east` is a task that only exists on the east lake square (39) to
#     climb back out onto the jetty.  `go fishing` scores only on square 43.
#   * The penknife MUST be closed before `climb down` at the steep decline:
#     tasks 12 and 13 have identical commands, and 13 -- taken when the knife is
#     open -- is `ACT type=6 v1=2`, instant death.
#   * A 30-turn bleeding clock (EVENT 0, started by `leave car`) runs until
#     `bandage self`; the route reaches the first-aid box with room to spare.
# The ending prints no score, so the route runs `score` (99/109) on the turn
# before the winning `smash window`, which banks the last 10.
# Needs SCR_SKIP_WAITKEY=1: the collapse at the front door ends in a "[Press
# any key]", which otherwise eats the first command in the guest room and
# desyncs the whole rest of the run.
thesisters_solution.txt|TheSisters.taf|lifeless body of Trisha Seabourne.|SCR_SKIP_WAITKEY=1
# The PK Girl (Robert Street, 2003) -- ADRIFT 4.00, 118 rooms, 2260 tasks,
# 29 NPCs, 187 variables: by a wide margin the largest game in this corpus.
# WIN in 407 commands with **Katryn 55 out of a possible 60**, ending on
# "Congratulations!  You got Katryn's ending.  Your Secret Letter is: E".
#
# There is no single score.  The game keeps eight independent relationship
# variables (VAR 158..165: laurie, cassie, saffy, monika, aileen, katryn,
# bengte, josie), each "out of a possible 60", and no task anywhere carries an
# `ACT type=4`.  A scoring task instead sets `change_score` (VAR 168) and then
# redirects to one of eight per-girl adder tasks, 2141 josie .. 2148 laurie.
# The ending is picked by TASKs 2211-2218, tested in the order Laurie, Cassie,
# Monika, Saffy, Aileen, Katryn, Bengte, Josie; the first girl with score >= 40
# AND `know_<girl>` set wins, and `name_of_girl` is then latched so no later
# test can fire.  Because Laurie is tested FIRST, courting two girls at once is
# actively harmful -- the route deliberately leaves Laurie at 11.
# Each ending prints one letter of the author password; the eight spell
# ICECREAM, which is also the .taf's own author password (Katryn's is E).
#
# downloaded/ThePKGirl_walkthrough.txt is a chapter-by-chapter command list
# that promises only "a basic ending" and courts nobody; it is the spine here,
# but every timed stretch in it is bracketed prose ("wait (for 37 turns, while
# Monika makes dinner)", "[walk around ... until you find the umbrella
# peddler]").  Nothing ships inside the game -- `hint` says hints are not
# available.  downloaded/ThePKGirl_hints.htm quotes a 45-point ending threshold
# where the tasks say 40; the tasks win.  Everything below came from the task
# table:
#   * Ch. 1 detour north to the bar and `talk to dustin` / `3` / `1` sets
#     `know_dustin`.  Skipping it costs 10 Katryn points at the very end, but
#     it also inserts an extra Dustin beat into the Ch. 4 cafe scene, so the
#     canned menu answers there have to absorb one extra turn (`wait` / `2` /
#     `talk to dustin`) or every later numbered answer lands one turn early.
#   * The peddler (NPC 26) WALKS a ~9-turn circuit around the plaza.  Every
#     turn added or removed anywhere earlier in the route changes his phase and
#     `give money to peddler` becomes "who do you want to give to?".  He is in
#     Center Plaza at the turn this route reaches it.
#   * In Research Lab C, `x machinery` is stolen by a generic scenery task;
#     `x equipment` is TASK 1847 and drops the two heavy magnets.
#   * `punch octal` on the silo needs task2009 "# Laurie fights back" -- six
#     turns after climbing out of the hatch.  Any earlier is "You are not close
#     enough".  Then `head butt octal` (+5) and `knee octal` (+5) both redirect
#     to 2025 "# Octal runs" and each requires it NOT done, so only ONE lands.
#   * The endgame window is exactly four turns wide: `get band`, then
#     `put band on octal` no later than the fourth, because TASK 2039
#     "# Katryn has a solution" fires on the fifth and takes the +5 away.  The
#     route spends the two turns in between on `kiss katryn` and `hug katryn`;
#     the kiss is refused (TASK 2135 is rep=0 and was already spent on the
#     warehouse kiss) but the hug is TASK 2134, +2.
#   * The +3 at the security-booth monitor is easy to miss: answering `2` then
#     `3` walks Katryn's talk state to 9 (state' = state*3 + n), which is the
#     one (situation 8, state 9) pair TASK 1234 pays for.
# Unreachable on this route: TASK 1684 "# Katryn advances" (situation 7) is an
# alternative to the warehouse kiss that pays +3 where the kiss pays +5, and
# the situation-10 +3 needs `katryn_done_talking` back at 0, which nothing
# resets once a conversation has closed.  55/60 is the practical ceiling.
# Needs SCR_SKIP_WAITKEY=1 -- the game is full of "Press enter to continue".
thepkgirl_solution.txt|the_pk_girl.taf|Your Secret Letter is: E|SCR_SKIP_WAITKEY=1
# Second Chance (David Whyld, 2005) replays its shipped Walkthrough.pdf
# VERBATIM -- 49 commands, not one repair, straight to the good ending.  The
# PDF is a full session log, so the command list falls out of it by taking
# every `>`-prefixed line; the only thing that needs knowing is that the title
# sequence embeds two <waitkey> pauses before the first prompt, so the row runs
# with SCR_SKIP_WAITKEY=1 (without it the first two commands are eaten and the
# whole run desyncs by two).
#
# The game keeps no score at all (`score` answers "No one's keeping score."),
# and its several endings are not ranked by points but by whether the three
# vignettes were played well: Dolores must call the police on the thugs
# (`push button`), Jenny must be talked to rather than pushed at (`3` then `2`,
# never "Ask her about sex"), Doug must be talked down three times, and
# Antonia's room must actually be searched.  The payoff is in the closing
# scene, where each person you saved turns up on the far side of the road --
# so the marker is the last line of that scene rather than a score line.
second_chance_solution.txt|second chance.taf|congratulating me on a job well done.|SCR_SKIP_WAITKEY=1
# Private Eye (David Whyld, 2006) is a pure numbered-choice game -- there is no
# parser verb in the whole route -- and the walkthrough section of its shipped
# 116-page "Private Eye.pdf" replays VERBATIM at the author's best ending,
# score 4 ("Better than Sherlock Holmes himself.").  73 choices, no repair.
#
# The one thing the PDF omits is the title menu: it opens at "The first thing I
# did was read through the file she had left me", which is already inside the
# game, so the script carries a leading `3` (Play Private Eye; 1 and 2 are the
# introduction and the notes).  SCR_SKIP_WAITKEY=1 is mandatory -- the game
# leans on <wait> constantly (the title sequence alone eats several), and
# without it the run never gets past the menu.
#
# One 12-word sentence in the PDF, "No sooner have I put the phone down than
# Jim ambles in.", has no counterpart in the run.  It is the author bridging
# two scenes in the write-up, not output we drop: the game's own wording is
# "...than Jim ambles in and plonks himself down in the other chair", it lives
# on the ex-girlfriend phone-call tasks rather than on Layla's, and the exact
# short form appears nowhere in the inflated .taf.  Word-diffed against the
# PDF the transcript is 19293/19371 words identical, and that sentence is the
# only difference.
private_eye_solution.txt|Private Eye.taf|You achieved a score of 4.|SCR_SKIP_WAITKEY=1
# The Plague - Redux is UNFINISHABLE as shipped, and -- like The Hangover
# above -- the defect is the author's Where/Type = 0 (ROOMLIST_NO_ROOMS), not
# ours.  The game's whole combat system is seven identical blocks of tasks,
# one per zombie encounter, and EVERY task in every block sits at where=0:
#
#   TASK 37 where=0 [f]                            <- "[F] Fight"
#   TASK 38 where=0 [e]                            <- "[E] Escape"
#   TASK 39 where=0 [*]                            <- the catch-all re-prompt
#   TASK 40 where=0 [#player wins attack round - fists 4]
#   TASK 41 where=0 [#player wins attack round - weapon 4]
#   ...
#
# 243 of the game's 696 tasks are parked at where=0.  Nothing ExecTasks the
# [f]/[e] pair, so once "[F] Fight or [E] Escape?" is printed there is no
# input that can answer it: `f` -> "That didn't make any sense!", `fight` ->
# "That wasn't the answer.", and `e` is eaten by the library as *east* ("The
# only exits were out.").  The first mandatory fight is the Women's Toilet
# cubicle, and the coins it guards are the last 10p of the GBP 1.20 the water
# vending machine wants -- so the route dead-ends there with GBP 1.10 and
# every later stage (Kate, the office vent, the camera batteries for the
# torch, Ray, the staff-area keys, the tunnels, Candice, the ending) is
# unreachable.
#
# Proved against the real ADRIFT 4 Runner, twice:
#   1. test/adrift4/harness/make_400_whereprobe.py builds a 3-task 4.0 game -- alpha at
#      where=0, beta at where=3, gamma at where=1/other-room.  run400.exe
#      fires beta, refuses gamma, and refuses alpha with the game's own "I
#      don't understand." -- i.e. it agrees with SCARE exactly on where=0.
#   2. A #StartRoom-patched copy of this very game (StartRoom 0 -> 15, the
#      Women's Toilets, repacked with taftool.py) driven in run400.exe under
#      Wine reaches the identical cubicle scene and answers `f` with "That
#      didn't make any sense!".  Same engine the author shipped for, same
#      refusal.
#
# So the row below is a maximal-reachable run, not a win: it replays the
# shipped walkthrough as far as it goes, types `f`/`fight`/`kill zombies` at
# the prompt to record the three refusals, collects all five reachable coin
# caches (rides, ticket windows, bench, condom machine, Thomas Cook desk),
# the cable, the trainers and the jacket, and ends at the vending machine.
# Two incidental notes: the game's discovery verb is SEARCH, not EXAMINE (the
# .doc says "Examine the till" but only `search till` works), and `x coins`
# is what counts the money -- `count coins` is not a verb here.  There is no
# score system at all (`score` prints the game's "notes" text), so the marker
# is the dead end itself.
plague_solution.txt|The Plague - Redux.taf|spilling zombie blood once|SCR_SKIP_WAITKEY=1
# ---------------------------------------------------------------------------
# 2026-08-04 -- IRVINE QUIK & THE SEARCH FOR THE FISH OF TRAGLEA (Duncan
# Bowsman, 2012, TAF "Version 3" i.e. 4.00).  179 commands, WIN, "THE END".
# There is no score system at all -- `score` answers "0 out of a maximum of 0"
# from the first turn to the last -- so reaching the epilogue is the only
# result there is, and the marker is the epilogue's opening line.
#
# The route follows the author's own iqsfot_walkthrough.pdf (12 pages, six
# chapters) and needs it: the game is a menu-driven six-chapter serial and
# several steps are not discoverable from the text.  Six PDF steps do not
# replay as written, all of them phrasing or a missing beat:
#   * `open hirby's compartment` -> `open compartment`, and only then does
#     `get papyr` parse ("Open what?" / "Take what?" otherwise).
#   * `x card` / `x card key` in chapter 4 is cosmetic and has no object
#     behind it ("Irvine sees no such thing"); dropped.
#   * `get hairball` is listed straight after `give flower to smitty`, but
#     giving the flower teleports Irvine to the INFIRMARY -- the hairball is
#     in the LABORATORY, so the route inserts `forward` first.
#   * the jungle exit `retreat, s, w, s, s` loses its first `w` to a
#     stalagmite trip at the CAVE MOUTH; the working form is
#     `retreat, s, w, w, s, s, s`.
#   * chapter 5's "fighting your way past any enemies" is the whole chapter,
#     and the PDF spells out none of it (see below).
#
# Chapter 5 is a real combat system and the reason this took a derivation
# rather than a replay.  From SCR_DUMP_TASKS (tasks 1217-1300, NPCs 16-20):
#   * Each of the four mooks blocks two attacks and folds to the other two --
#     sentry punch/kick, guard kick/sweep, patrol sweep/throw, soldier
#     throw/punch -- and the four counter-gated copies of each task (RESTR
#     type=4 on the Punch#/Kick#/Sweep#/Throw# variables) only rotate the
#     prose; every copy of a *correct* attack does the same KO.
#   * There are only four mooks in the whole palace -- one NPC each -- and a
#     KO is not the end of any of them.  EVENTs 15-18 [Sentry/Guard/Patrol/
#     Soldier Respawn] restart each one on its own timer (7 / 9-14 / 6-10 /
#     9-11 turns) into whatever room the player is standing in, which is what
#     "A sentry charges in" and "Patrol charges after Irvine" are.  The
#     palace therefore cannot be cleared, only outrun: the four timers are
#     staggered, so a room is empty for a turn or two at a time and the route
#     has to spend that turn moving.
#   * Leaving is blocked while anything is in the room ("Irvine has to deal
#     with his enemies before he can leave!"), so every doorway costs a full
#     sweep of whoever has cycled back in.  That is what lines 134-138,
#     144-149 and 155-159 are; the doubled `sweep patrol` / `kick guard` are
#     the respawn landing on the very turn of the KO, not a missed swing.
#   * `claw` (TASK 1217) is an area attack that hits every enemy present at
#     once, and it is the ONLY thing that touches the elite -- TASK 1292
#     `#elite_clawed_(POW!)` carries four "NPC not in room" restrictions, one
#     per mook, which is the "elite must be alone" rule from the PDF.  It is
#     gated on `claw_count >= 3` and resets the counter to 0, and every
#     attack (hit or miss) bumps the counter by one, so it recharges over
#     three swings.  The route saves its one charge for the elite and takes
#     the throne-hall door on the very next turn.
#   * Health is a damage counter, not a pool: VAR 41 [Irvine_Health] starts a
#     fight at 0 and each `#<mook>_attack` adds 1 for every enemy standing in
#     the room; VAR 63 [HP] is only the mirror (TASK 1342 ###IRVINE_HEALTH###
#     dispatches TASK 1349-1361 #IrH0..#IrH12, each setting HP = 12 - damage).
#     At damage 12 TASK 1343 #Irvine_LifeCheck fires, and inside the palace
#     (rooms 42-53) TASK 1347 #imprisoned! throws Irvine in room 62.  TASK
#     1348 #heal_over_time takes one damage back off and EVENT 45 [Heal Over
#     Time] runs it every 3-6 turns.  The route arrives at the throne hall on
#     HP 9 and finishes chapter 5 on 9.  `breathe` (one damage off a turn,
#     refused unless Irvine is alone) is the only repair the player can aim,
#     and it does work -- 9 back to a full 12 in three turns -- but the respawn
#     lands on the fourth quiet turn wherever the player is, so topping up
#     just hands the wave back at the wrong moment; going straight through
#     turned out to be cheaper than healing first.
# The two health-restoring objects the tasks talk about are unreachable: the
# health pill (obj310) has no Where node at all and no action anywhere moves
# it, so OBJLOC reports pos=-1 room=-1 for the whole game.
#
# The chapter 6 fan-servant scene (`teach fan karate`, `give jacket to fan`,
# `ask for help`) is optional by the PDF's own admission; it is kept because
# the epilogue calls back to it ("Where's your coat?" / "Gave it away.").
iqsfot_solution.txt|iqsfot.taf|Thus one courageous space cadet saved the fish|SCR_SKIP_WAITKEY=1
# ---------------------------------------------------------------------------
# 2026-08-04 -- MANGIASAUR (DCBSupafly, ADRIFT Spring Comp 2011).  You are a
# dinosaur and the entire verb set is EAT.  87 commands, WIN, 63/74.
#
# The engine facts behind the route, from SCR_DUMP_TASKS:
#   * win  = TASK 177 `eat platter`, which chains TASK 178..186; TASK 186 is
#     the `ACT type=6 v1=0`.  The platter is put in the Hall of Humans by
#     TASK 176, fired by EVENT 30 the moment TASK 147 (`down` off the mesa)
#     completes.  TASK 187 `eat human` is a second, cheaper ending -- never
#     type it, this route is well past its size>60 gate.
#   * 63/74 is the ceiling, not a shortfall.  Two of the 74 points cannot be
#     scored by anybody:
#       - TASK 76 `eat NAMGUAGL` is worth 10 and its object (obj15) is never
#         placed anywhere.  OBJLOC says pos=-1 room=-1 at load, the only
#         action in the whole game that touches obj15 is TASK 76's own
#         `ACT type=0 v1=7 v2=0 v3=0` (which *hides* it), and no event moves
#         any object at all (every EVENT line is `o2=0->0 o3=0->0`).  The two
#         warning tasks and DEATH BY NAMGUAGL are dead code for the same
#         reason.
#       - TASK 123 `eat mutilated carcass` carries two `ACT type=4` actions
#         (+5 and +1) and this is a TAF 4.00 game, so
#         `task_run_change_score_action` awards only the first.
#   * Eight counter variables are declared and read but never written by any
#     `ACT type=3`: eatenMoths, eatenBugs, eatenMoss, eatenHoppers,
#     eatenBuzzBirds, eatenBushes, eatenRoots, hunterHasSpear.  Two of the
#     eight ending "you taste..." paragraphs (TASK 179 moss, TASK 185 roots)
#     are gated on two of them and can therefore never print.
#   * The air sac is a one-shot fuse, and that is the whole reason for the
#     five `eat air sac` in a row near the end.  `eat air sac` (TASK 86) sets
#     carcassEdible=1, which does double duty: it suppresses the ocean drown
#     timer (TASK 101 only runs when carcassEdible==0) and it is the gate on
#     TASK 123.  EVENT 17 has restart=0, so the *first* sac you ever eat
#     starts a 10-20 turn countdown that runs exactly once and ends by
#     running TASK 88, which sets carcassEdible back to 0 (and drowns you if
#     you are still in the ocean).  Eat one sac, dive, surface, then keep
#     eating sacs until that one-shot has fired -- after it has, the next sac
#     sticks for good and the carcass is edible on the mesa.
#   * `burp on sap` (TASK 162) is not a door-opener, it is the ride: it moves
#     the player straight to the Mesa Top.  It needs canBurp, which comes
#     from eating the hut's lit torch.
mangiasaur_solution.txt|Mangiasaur.taf|Thanks for playing Mangiasaur!|
# ---------------------------------------------------------------------------
# 2026-08-04 -- A FINE DAY FOR REAPING (James Webb / revgiblet, IFComp 2007).
# You are Death, and five souls are due today.  Each soul has two or three
# independent solutions -- the author's own walkthrough lists them all -- so
# there is no canonical route; the one below picks the cheapest branch for
# each and reaps all five in 73 moves.  There is no score system (`score`
# prints 0/0), so the marker is the last line of the ending text.
#
# The engine facts behind the route, from SCR_DUMP_TASKS:
#   * win  = TASK 6, fired by EVENT 2 when the variable `soulsreaped` hits 5.
#   * loss = TASK 5, fired by EVENT 1 when `timea` reaches 47.  EVENT 0 bumps
#     `timea` every 15 turns, i.e. the twelve in-game hours are a ~705-turn
#     budget.  73 moves spends two of them -- the hourglass still says "ten
#     hours" at the end -- so this is nowhere near the timer.
#   * The horse only listens in the hub rooms: every `say <place> to horse`
#     task carries WHERE_ROOMS=[5 6 7 13 17 25 34 39 40 41 42 46 51].  From
#     anywhere else you get "No-one pays any attention to you", which is why
#     the route walks back out to the Storage Cupboard before travelling.
#   * The arrival auto-moves (e.g. TASK 70, Kenya -> the Hut) are rep=0, so
#     they fire on the *first* visit only.  The second Kenya trip lands in
#     the Village and needs an explicit `n`.
#   * `take tape` is refused on purpose ("If you ever need it then you know
#     where to find it") -- the masking tape is consumed implicitly by
#     `repair shovel`, which itself requires `x workbench` first (TASK 182).
afdfr_solution.txt|AFDFR.taf|Life is good for Death.|SCR_SKIP_WAITKEY=1
# ---------------------------------------------------------------------------
# 2026-08-03 -- THE COMPLETE TAF 3.80 CORPUS.  A byte-level survey of both
# hosts that still carry ADRIFT games (every .taf on ifarchive.org
# /if-archive/games/adrift/ including the ones inside zips, and every download
# in the adrift.co adventure DB) turned up eleven ADRIFT 3.80 games.  Neither
# IFDB nor IFWiki can be used for this -- their oldest ADRIFT format/category
# is 3.9, so 3.8 games are filed as 3.9 everywhere; the only reliable test is
# the 14-byte header, which is "Version X.YZ\r\n" XOR the fixed VB6 keystream
# and therefore a constant per version (3.80 = 3c423fc96a87c2cf94453661 39fa,
# see the V380_SIGNATURE table in sctaffil.cpp).  A `Range: bytes=0-13`
# request classifies a remote .taf without downloading it -- but adrift.co
# sometimes answers a ranged GET with an empty body, so a short reply must be
# retried unranged or the file gets misfiled as "unknown".
#
# marooned / wrecked / Crime_Adventure (above) were the first three.  The
# remaining eight are now in games/ as well, so the 3.8 burden and container
# model settled against run380.exe can be exercised across the whole corpus
# rather than the three games it was derived on.  Provenance:
#   akron cave haunt twilight   ifarchive.org/if-archive/games/adrift/<f>.taf
#   haunted great secret tra    www.adrift.co/files/games/<f>.taf
# (the four adrift.co-only ones are on no other public host).  All eight load
# and run in `scare`; each row below SKIPs nothing and reports NOSCRIPT until
# its route is derived -- see WALKTHROUGH_TODO.md for the queue.  Win markers
# are left empty deliberately: filling one in before the route exists would
# bless a marker nobody has seen the game print.
#
# 2026-08-04 -- SIX MORE, and the reason eleven was too low: adrift.co serves
# files that its adventure DB never lists, so enumerating the DB misses them.
# The name list comes from the Wayback copies of Campbell Wild's own game
# pages (tardis.ed.ac.uk/~jcw/{adventure,adrift}/adventure.html 2000-2001 and
# jcwild.pwp.blueyonder.co.uk/adrift/adventure.html 2001-2002); probing those
# 130 historic filenames against www.adrift.co/files/games/<f> found six more
# 3.80 games, none of which is on the IF Archive or in the DB:
#   duck (Duck McCloud), first (The book of Fistandantalus), jb2000 (James
#   Bond - Happy Landings), microwaveman (Microwave Man!), mikes (The life of
#   Mike), superliam (Super Liam 1).
# All six load and run.  Seventeen 3.80 games are therefore known to survive.
# The same sweep found the only two surviving pre-3.80 games -- arlo.taf
# (Alice's Restaurant Anti-Massacree Adventure, Laura Lee, 18-03-2000) and
# castle.taf (Castle Quest, Andrew Cornish, 10-06-2000), both **Version 3.70**
# (header 3c423fc96a87c2cf94453961 39fa).  Nothing 3.60 or older survives.
# Scarier gained a 3.70 schema on 2026-08-04 (V370_PARSE_SCHEMA in
# sctafpar.cpp, every guessed field since measured against the real
# run370.exe), so both now sit in games/ with the rest of the corpus and both
# load and run -- see ../ADRIFT_370.md and RUNNER_TESTS_TODO.md section 6.
akron_solution.txt|akron.taf|you brave adventurer, saved yourself
cave_solution.txt|cave.taf|Suddenly, in front of you, the rock face slides open revealing an enormous
haunt_solution.txt|haunt.taf|You drop down into the laboratory, Horace following you
twilight_solution.txt|twilight.taf|Your score is 500 out of a maximum of 500
haunted_house_solution.txt|haunted.taf|You went in, you got the treasure, and now you
great_escape_solution.txt|great.taf|cry of joy, you have made it, you have escaped!!
tom_ceader_solution.txt|secret.taf|you did good work escaping from the town
timmy_reid_solution.txt|tra.taf|Thanks for getting us back home!
duck_mccloud_solution.txt|duck.taf|You jump from the plane just in time and you survive the huge
fistandantalus_solution.txt|first.taf|Congradulations you have won the game
james_bond_solution.txt|jb2000.taf|YOU COMPLEATED THE MISSION! YOU LANDED WELL
microwave_man_solution.txt|microwaveman.taf|You have destroyed Coffee Man
life_of_mike_solution.txt|mikes.taf|Ypu ask her out
super_liam_solution.txt|superliam.taf|congradulation you have defeated x1
alices_restaurant_solution.txt|arlo.taf|recording an album that will be that hit record
castle_quest_solution.txt|castle.taf|Thanks for playing!
# ---------------------------------------------------------------------------
# 2026-08-04 -- the `downloaded/` wiring run resumes: the ten walkthroughs in
# downloaded/ whose .taf was already staged but which had no route.
#
# The Dead Man (30otsix, 2003) is a one-room countdown piece whose every
# blackout vision is a <waitkey> pause, so it is wired with SCR_SKIP_WAITKEY=1
# (as afdfr is) -- without it the delron command list needs ~18 blank filler
# lines scattered through it and every vision eats the command behind it.
# Two further corrections to the published list: the blackouts make you *drop
# everything*, so the panel and the bandage have to be done before the first
# one (turn 15), and its 26 waits are 23 here.  The published route stops at
# 32/43; this one takes the three scoring actions it skips -- `open panel with
# scissors` (+5), `wear bandage on neck` (+3) and the security camera (+1) --
# for **41/43**.  The last 2 points are TASK 19, `shoot myself`, which is a
# death, so 41 is the winning ceiling.
deadman_solution.txt|The Dead Man.taf|ABORT SUCSESFUL|SCR_SKIP_WAITKEY=1
# Ba'Roo! -- delron's own command list, +2 lines: the capsule wants the
# backpack *inside* it (TASK 258/286 restrict obj1 to "in capsule"), and the
# suit puts the backpack back on you, so "remove backpack" has to precede both
# "put backpack in capsule".  16/16, the game's own maximum.
baroo_solution.txt|baroo.taf|Congratulations!
# Lair of the Vampire -- the author's own 276-line command list plus 3 lines.
# The ruined stairs (rooms 11/14) are a coin flip: TASK 140 carries you up only
# while the `stairs` variable is < 3 and TASK 139 rerolls it every turn, so the
# published single `up` has to become `up`/`up` (under the harness seed the
# second try lands).  The other two are a gap in the published list: it jumps
# from `east` straight to `ne`/`ne`, but the two `ne`s are Feasthall->Corridor
# ->The Statue, so `east`/`north` are needed first to reach the Feasthall.
# Wins at 226/271 (83%) -- the game itself says so on the last screen ("There
# are a good number of tasks you can complete which add to your score but which
# are not required to complete the game"), so this is a winning, not a maximal,
# route.  Needs SCR_SKIP_WAITKEY=1: the intro, the Deathly Chamber archway and
# the ending all paginate.
lair_solution.txt|Lair of the Vampire.taf|the lord of the vampires, lies dead|SCR_SKIP_WAITKEY=1

# The Fugitive -- derived from scratch (the downloaded walkthrough is prose-only
# and stops at the city gate).  656/666, which is every point the game can
# actually award: the only unreachable scorer is TASK 73 [look * mirror] in the
# three drivable cars, and the game's OWN input synonym rewrites the word
# "look" to "l" before task matching, so that pattern can never fire (verified
# by SCR_TRACE_FLAGS=512: `look in mirror` reaches the matcher as
# `l in mirror`).  Route notes that cost the most digging:
#   * Take the TAXI, not "my car".  Both dump you in the same street maze, but
#     only the taxi lets you `fight` the driver (TASK 27, +10) and keep his
#     pistol, and only with a pistol in hand does the punker ambush in streets
#     <12> resolve as TASK 35 instead of TASK 33 -- 35 leaves a dead punker
#     carrying the can of beer that TASK 37 [drink * beer] wants (+10).
#     The car's only exclusive scorer is the dead mirror task, so it loses 20-0.
#   * Drink the beer BEFORE boarding the train: `jump out` of the moving train
#     runs TASK 29's "drop everything" action and the can goes with it.
#   * The train is on a 3-turn cycle; board on the turn AFTER "The train is
#     coming to station".  Any change to the route length upstream re-phases it.
#   * In the woods take the boots first and `undress soldier` LAST: EVENT 26
#     arms "death in woods2" six turns after the undress, and the walk to the
#     jeep plus the guard-kill wait is five.
#   * `dive` for the fountain coins -- the authored pattern is [get * coin*],
#     but the game also declares the synonym coins->money, so `get coins`
#     arrives at the matcher as `get money` and is answered by the library.
#   * The casino slot machine SETS your money to a random amount (2181 -> 56 on
#     this seed), so `play` has to come after `buy bomb`, not before.
#   * `sleep` needs you horizontal (`lie on bed`) and flips the clock to 23:00,
#     which is what turns the night-only half of the city on (bar concert,
#     nightclub, casino, Thel, the church shadow) and the day-only half off.
#   * The Thel scene teleports you to Riverside Road <2>; walk back to Outside
#     the library for `x thel` (+10) before moving on.
#   * Do the church interior BEFORE `unlock`: unlocking arms EVENT 46, which
#     kills you in the cemetery five turns later, and shovel/dig/seal is four.
fugitive_solution.txt|Fugitive.taf|This is the proof of innocence|SCR_SKIP_WAITKEY=1

# --- 2026-08-04: the six games whose downloaded/ source is a ClubFloyd log or
# a hints file rather than a command list.  None of the six had a route; all
# six are derived here against the task dumps (the ClubFloyd logs are group
# play sessions -- they wander, die, undo and re-enter, and four of them never
# reach the ending at all).
#
# Mammoth Vacuum Button of Death (Daniel Airey, New Year's Speed IF 2012) is eleven commands long
# and the whole game is one joke: `strip` (yourself) and `strip guard` are two
# different tasks and you need both, because the guard's uniform is the only
# way past the foyer.  SCR_SKIP_WAITKEY=1 for the dream intro and the ending.
mammoth_solution.txt|MammothVacuum.taf|After many testing trials|SCR_SKIP_WAITKEY=1
# I Was a Teenage Headless Experiment (Duncan Bowsman, EctoComp 2010, 4th).  Ten commands.  The
# waitkey flag is mandatory for a reason that is easy to misread: the game
# OPENS with a fake death -- a joke "you are already dead" screen with a
# <waitkey> on it -- so without the flag the very first command is eaten and
# the route silently walks a different game.  The one real puzzle is that
# `put head on body` (TASK 57/58) restricts Formula X to *held*, not merely
# present, so `get syringe` has to follow `kill gerchis`; the ClubFloyd log
# gets stuck here for pages.
headless_solution.txt|headless.taf|as a teenage headless experiment|SCR_SKIP_WAITKEY=1
# Cut the Red Wire! No, the Blue Wire! (David Whyld, InsideADRIFT #41,
# 2012) -- a one-move joke
# game whose winning move is `undo`.  That works because a game task beats the
# standard library: run_game_commands_in_parser_context() is called before
# run_standard_commands() (scrunner.cpp ~1616), so the authored [undo] task
# fires instead of lib_cmd_undo.  Cutting either wire kills you; so does doing
# anything else, on a one-turn fuse.  Scores 1/1, the maximum.
# The game has NO game-over action at all -- it prints the ending, then loops
# back to the warehouse -- so the transcript keeps going past the win and the
# appended `quit`/`y` are answered by the game, not by the library.  That is
# in the golden on purpose; the win marker is the score line.
redwire_solution.txt|Cut_the_Red_Wire.taf|a maximum possible of 1. Well done.|SCR_SKIP_WAITKEY=1
# I am the Law (djchallis, The Odd Competition 2010, 2nd).  No score, so the ending is the
# only measure.  The endgame is a small variable machine: `make verdict` sets
# verdict=2, then naming the culprit sets verdict=4 if it was V (the ship's
# computer) and verdict=3 for anyone else, and `mission` wins on verdict==4
# and loses on any verdict>=3.  So the mechanical win is three commands.  The
# wired route does the actual investigation first (the body, Seth's diary and
# the 4th November entry, Calvin on the creativity engine, Luke for its
# password, `enter creativity password` + `grant`, then William on what is
# behind the curtain) because that is what the ClubFloyd session is playing
# and the transcript is worthless without it.  The password prompt is its own
# little state machine: TASK 5 needs variable 4 == 1 and sets it to 2, TASK 6
# (`grant`) needs 2 and sets 3, and TASK 7 -- pattern `*`, i.e. literally
# anything else -- resets it to 1, so a wrong guess drops you out silently.
law_solution.txt|I am the Law.taf|Congratulations!|SCR_SKIP_WAITKEY=1
# In Memory (Jacqueline A. Lott, Indigo New Language Speed IF 2011).  Fifteen commands.  Not a puzzle game: you are
# an unconscious dying person named Alex and the whole of it is TASK 178,
# `EndGameScene`, gated on `RESTR type=4 v1=2 v2=2 v3=7` -- variable 0 == 7.
# Each of the seven memory rooms (rabbit / desk / outfit / Sam / headphones /
# book / vista) has a swarm of one-shot answer tasks; ANY answer that matches
# one of them sets its text variable, bumps variable 0, and walks you back to
# Unconsciousness <2>.  So the route is `let go` (room 0 -> room 1) and then
# seven noun/answer pairs, and the answers chosen here are simply the first
# option of each set (happy / english / casual / smile / rock / fantasy /
# mountains) -- any other legal answer wins too, with different prose.
inmemory_solution.txt|InMemory.taf|had ceased to beep.|SCR_SKIP_WAITKEY=1
# Happy Valley (Jacqueline H. as "Lumin", 2008-07-02) -- downloaded/HappyValley_hints.txt IS a
# command list, but it does not run: it is written against a later revision.
# `x path`/`x patch`/`x weeds` are listed at Outside the Mine, but the patch
# and the weeds are objects 96/97 in room 0 (Happy Valley); `n`/`s` are listed
# where room 2 only has E and W; `enter 3436` cannot match TASK 56's pattern
# `enter 3436 *`; and `turn on water`/`water plant` are listed before the cup
# is filled.  This route is the same solution re-derived against the dump.
# The one restriction that constrains it hard is TASK 46 (`give cup to
# granny`): `RESTR type=4 v1=2 v2=2 v3=5` is variable 0 == **5** exactly, so
# the five potion ingredients must all be given and the decoy pink spotted
# leaf must not be (TASK 40 accepts it but does not increment).  Also: the
# gloves must be WORN, not carried, for TASK 36 (the demonflower bites); and
# `x tools` in the smithy is what places the crowbar there, so it has to
# happen on the one visit, before `n` teleports you out with the sword.
valley_solution.txt|valley.taf|and live happily ever after.|SCR_SKIP_WAITKEY=1
# The five games whose .taf files arrived on 2026-08-04 (see WALKTHROUGH_TODO.md
# "2026-08-04 (later)").
#
# ImagiDroids: the upstream Woodfish-compendium list replays verbatim but for
# `open it` -> `open brick` (the pronoun still points at the clean area, so the
# key never appears).  Its `north` used to fail too, and that one was the
# interpreter's fault: TASK 38 is `{go/walk/move}[n/escape/out]{orth/out}`, a
# word built out of two adjacent groups, and SCARE required a space between
# adjacent [] / {} groups.  Fixed 2026-08-04 in scparser.cpp (NODE_JOIN);
# TASK 6's `[s]{outh}{ /-}[w]{est}` is the proof of intent, since the space in
# "south west" is spelled out as an explicit alternative.  No score system; the
# single ending is EVENT 5 -> TASK 42 -> ACT type=6.
imagidroids_solution.txt|imagi.taf|You choose to put him out of his misery.|SCR_SKIP_WAITKEY=1
# Crimson Detritus: the shipped transcript replayed, 100/100 (all eight ACT
# type=4 in the game), with `take uniform and wear it` split into two commands
# -- the transcript prints two responses to that line, so it was two commands
# in the original session, and SCARE reads the single line as
# `take uniform and wear the hook`.  The endgame prints three literal "{}"
# sequences that the author's transcript does not show; they really are in the
# game text (see the solution header).
crimsondetritus_solution.txt|CD.taf|until the next victim comes along to take your place.|SCR_SKIP_WAITKEY=1
# Chosen (ADRIFT 3.90, MiniComp 2001): 300/300, the game's own stated maximum.
# The upstream file is the author's prose hint sheet, not a command list, so the
# route is derived from the game.  Three things it has to get right, each of
# which is a death or a dead end: `pull lever` in room 8 before room 7 (TASK 7
# is restricted on TASK 8; unrestricted TASK 9 is the same command in the same
# room and feeds you to the tiger), never any form of `take block` in room 14
# (TASK 14 = ACT type=6 v1=2 -- the block comes off the pillar via the string
# and `up`), and the six blocks plugged in the order A, D, R, I, F, T, since
# TASK 18-22 each restrict on the previous one.  The blocks answer only to
# their full names ("take a-shaped metal block").
chosen_solution.txt|Chosen.taf|You plug the T-shaped block into the final socket in the door.|SCR_SKIP_WAITKEY=1
# The Cellar (David Whyld, 2007): the ClubFloyd session of 12 June 2022 replayed
# verbatim, all 132 commands including the typos, the dead ends and four
# `undo`s.  No repairs.  The game also ships its own 24-command walkthrough on
# TASK 1 (`walkthrough`), which likewise replays verbatim, so the solution path
# is confirmed twice from independent sources; the ClubFloyd route is the one
# wired because it is the downloaded file and it reaches far more of the game's
# 141 tasks.  No score and no ACT type=6 -- the game ends via VAR 12
# [game over], so the marker is the ending line.
cellar_solution.txt|TheCellar.taf|And so The Cellar has ended. Many thanks for playing.|SCR_SKIP_WAITKEY=1
# Panic! (Stewart J. McAbney, ADRIFT 3.90): the author's own walkthrough
# transcript replayed verbatim, all 69 commands, no repairs.  The first command
# is `1`, because the game opens on a menu rather than in a room.  40 of a
# maximum 60 -- the author's shortest path is not a full-score path -- and the
# ending is a task chain with no ACT type=6, so the marker is the rating line.
# This is the game that found the 3.9/3.8 immediate-restart bug: its ambient
# events are RestartType=1 with Time1=Time2=1 and StartText but no LookText,
# and scarier printed each of them exactly once instead of every turn.  Fixed
# 2026-08-04 in scevents.cpp; see the solution header and RUNNER_TESTS_TODO.md
# section 8.
panic_solution.txt|panic.taf|Your rating is Messiah.|SCR_SKIP_WAITKEY=1
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
SRC_DIR="${SCARE_DIR:-$(cd "$HERE/../../.." && pwd)}"
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
  solpath="$HERE/../goldens/$sol"
  golden="$HERE/../goldens/${sol%.txt}.expected.txt"

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
