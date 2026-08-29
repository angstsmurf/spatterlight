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
icecream_solution.txt|IceCream.taf||SCR_SKIP_WAITKEY=1
# Measured live in run400 under Wine (2026-08-24), full 8-command replay,
# Verbose ON: the Runner prints only "Huey the Contractor walks by and
# stops."  It never prints "A young boy arrives under the tree." -- the boy
# NPC's walk has expired by then.  The old golden carried the arrival, so it
# was wrong; every other line of the replay matched.  See the NPC walk-ticker
# rules in scnpcs.cpp npc_tick_npc().
the_cat_in_the_tree_solution.txt|TheCatintheTree.taf|You scored 50 out of the maximum 50!|SCR_SKIP_WAITKEY=1
man_overboard_solution.txt|man overboard.taf|Maybe it wasn't all a waste of time|SCR_SKIP_WAITKEY=1
pieces_of_eden_solution.txt|Pieces of eden.taf|END OF PART ONE
princess_in_the_tower_solution.txt|princess1.taf|It seems you've won.
too_much_exercise_solution.txt|exercise.taf|much prefer that Sweet Shop option one of your work colleagues took.
yak_shaving_solution.txt|yak_shaving.taf|completed the Odd Competition|SCR_SKIP_WAITKEY=1
buried_alive_solution.txt|buried.taf|Well done. You got to the end
confession_solution.txt|Confession(1).taf|Striking a plea deal|SCR_SKIP_WAITKEY=1
snakes_and_ladders_solution.txt|sandl.taf|made it to the end of the game|SCR_SEED=2
# Re-blessed 2026-08-25, one line, for the pre-4.0 `x <unknown noun>` answer.
# 4.0 rewrote the last line of the Runner's examines(): pre-4.0 answers the
# flat, person-free "Nothing special." (run370 435BF4, verbatim in run370's
# Form1.frm), 4.0 answers "<player> see no such thing." (run400 471EF6).  The
# exes date it -- ' see no such thing.' is in run400.exe and in none of
# run370/380/390.exe, while 'Nothing special.' is in all four.  Both halves are
# measured live: run390 on Merry_Murders.taf (3.90), Adrift_39_merry_murders.txt
# line 38, `x pocket` in the lit Plaza with no `pocket` object -> "Nothing
# special."; run400 on The_X-Files_A_New_Beginning.taf (4.00),
# Adrift_22_xfiles.txt lines 187/233, `look at camera` / `look up byers` ->
# "You see no such thing."  veteran, zombies and everything are the only three
# goldens in the corpus that reach the line, and all three are 3.90.
veteran_solution.txt|veteran.taf|fulfilling your destiny
togetyou_solution.txt|togetyou.taf|another flesh-sack|SCR_SKIP_WAITKEY=1
# Re-blessed 2026-08-25, two lines, for the pre-4.0 `x <unknown noun>` answer
# measured on the veteran row above.
zombies_solution.txt|ZAC.taf|you and Stu were eaten by zombies|SCR_SKIP_WAITKEY=1
adrift_maze_solution.txt|ADRIFTMaze.taf|You WIN!
cruel_solution.txt|CAH.taf|destroyed our reality
trabula_solution.txt|Trabula.taf|given the gold coins to Trabula
shred_em_solution.txt|shreddem.taf|Due to lack of evidence
shadowpeak_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=7
shadowpeak_allgargoyles_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=83
shadowpeak_killwraith_solution.txt|Shadowpeak.taf|completed the adventure Shadowpeak|SCR_SEED=48
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
#
# Re-blessed again 2026-08-25, four lines across the three rows, for the
# object-name article strip.  These are the only goldens in the corpus that
# reach it: Shadowpeak has two objects with an empty Prefix whose Short name
# opens with an article -- [The horn of the angels] and [The dead Margo] --
# and scarier used to strip that "The" off the name as well as normalizing
# the prefix, printing "You take the  horn of the angels." with the tell-tale
# double space.  No Runner can do that.  run400's object loader
# (mdlSpreadTheLoad.bas:7594-7653) rewrites an empty Prefix to a literal "a",
# trims trailing spaces off it and leading spaces off the Short, and looks at
# nothing else; the name builder Proc_21_31_448710 then hands
# `Prefix & " " & Short` to tense, which only ever tests the head of that
# string.  So the Runner prints "the The horn of the angels", and the same
# shape is confirmed live in the xfiles replay, where a "The" prefix survives
# as "The Memo" (Adrift_22_xfiles.txt).
# The Forester Goblin fight lost two of its three `attack goblin` commands, and
# the row gained SCR_SEED=4, when the dead-NPC walk skip landed (2026-08-24,
# see notes/WINE-TRANSCRIPTS-TODO.md): the goblin's corpse used to keep
# walking and so stayed attackable, and every kill in the game now consumes
# one `scr_randomint` less per turn (a room-group walk draws a random
# adjacent member), which moves the combat rolls under the old seed far
# enough that Urgorn wins.  Seed 4 is the lowest at which the trimmed route
# beats him with no unparsed command anywhere in the replay.
alexis_solution.txt|ALEXIS.TAF|you have beaten Urgorn|SCR_SEED=4
alexis_worn_cube_solution.txt|ALEXIS.TAF|you have beaten Urgorn
topaz_solution.txt|topaz.taf|The two of you set out into the forest.|SCR_SKIP_WAITKEY=1
thorn_solution.txt|Thorn.taf|You have chosen to look upon your own mortality.
# ---------------------------------------------------------------------------
# THE OBJECT *SEEN* MODEL (2026-08-24).  An object is not referenceable until
# something LISTS it.  The Runner stamps the seen byte in its room lister and
# in its examine/open/inventory paths -- never once a turn -- so a room the
# game never described hides everything loose in it, and `take <thing>` there
# answers "Take what?".  Two live run400 measurements pin this:
#   * The X-Files, Adrift_22_xfiles.txt lines 92-93: `take knife` -> "Take what?" in
#     Garage 5, entered through task 7 ("Use Key", ShowRoomDesc = 0), with the
#     Small Pocket Knife lying loose on the floor.  The next command, `out`,
#     moves normally, so the player really is standing there.
#     (2026-08-25: read that transcript exactly as it stands.  The Runner got
#     `use key` and then `take knife` with NOTHING between them -- the `look`
#     the walkthrough puts there was lost by the feed and never echoed.  That
#     is what makes the measurement clean, but it also means the reveal command
#     was never tested here; and the knock refusal eleven commands later is not
#     a second divergence, just task 9's fourth restriction, "Knife held".)
#   * humbug, Adrift_29_humbug.txt: `X teeth` -> "Nothing Special." (see that row).
# Fifteen rows were re-derived for it, each by inserting the reveal command a
# player would actually type (a `look`, or an `x` of the container) before the
# first reference; where that turn could not be spent -- colony's alien kills
# in two hits, 3monkeys' mandrill corners you -- an existing turn was folded
# into it instead, keeping the route turn-for-turn identical.  Every one of the
# fifteen still wins, and lair still scores its full 226.  The engine change is
# obj_mark_room_objects_seen()/obj_mark_room_statics_seen() in scobjcts.cpp.
# ---------------------------------------------------------------------------
# Renegade Brainwave: `west` is a task move with ShowRoomDesc off, so the yew
# tree's crowbar is never listed; the route gained one `look`.
renegade_brainwave_solution.txt|Renegade_Brainwave.taf|planet Earth has been averted!
# ---------------------------------------------------------------------------
# MEASURED 2026-08-25 on run400 (Adrift_1_goldilocks.txt), 252 commands, all
# 252 echoed, and it found the event look-text room gate.
#
# Turn 243, `u` -- the escape from the flooding cellar.  The task that lifts
# you out has ShowRoomDesc set to the hall, and a ShowRoomDesc room is printed
# BEFORE the task's own actions run (see adrift4-showroomdesc-before-actions),
# so at print time the player is still standing in the cellar.  Scarier gated
# the event look-text loop on the player's room and spliced EVENT 4 [Cellar
# fills with porridge] -- Where = some rooms {cellar, dark passage, dungeon} --
# into the description of the hall:
#
#   run400   ...front door is an open trapdoor.  I can move north, west, up,
#            down and out.
#   scarier  ...front door is an open trapdoor.  Extremely hot porridge is
#            gushing down the sides of the porridge pot.  The room is steadily
#            beginning to fill up with the stuff.  I can move north, west, up,
#            down and out.
#
# Both Runners index the event's room list with the room they were ASKED to
# describe: run400 viewroom at loc_472B53 (`arg_C - 1`), run390 viewroom at
# loc_448075 (`broom - 1`), each ANDed with the running-state byte (74 in 4.0,
# 70 in 3.9).  Fixed with evt_can_see_event_in_room(); after it, 251 of the 252
# turns are byte-identical and the 252nd differs only by [Press any key to end],
# which is a waitkey mark by design.
goldilocks_solution.txt|goldilocks.taf|Three Bears are no more
masochists_heaven_solution.txt|1HRGAME.taf|You scored 15 out of the maximum 15!
griswold_solution.txt|Griswold.taf|And there you have it: the intro|SCR_SKIP_WAITKEY=1
mhpquest_solution.txt|mhpquest.taf|You have saved Crystal's life
# Archie's Birthday is AIF: the game's text is sexually explicit, so its solution
# and golden are deliberately NOT committed (they are in harness/.gitignore).  The
# row stays so the regression runs where the files exist; elsewhere it NOSCRIPTs.
archie_solution.txt|Archie's Birthday V 1-2.taf|To be continued|SCR_SKIP_WAITKEY=1
# The adrift-battle corpus (the WALKTHROUGH_TODO.md games, banked 2026-06) --
# wins first, then documented-max tours / sandboxes / demos.  Tour rows use the
# final "Your score is N out of a maximum of M." line as their marker so the
# documented maxima stay locked; win rows use the game's own victory text.
bomb_threat_solution.txt|Bomb Threat.taf|Or have you...
# circus's three "The vendor ..." walk lines are the corpus proof that the
# announcement is joined into the turn's paragraph: the author carries the ALR
# pair '  Joe' -> '  The vendor' / 'Joe' -> 'the vendor', and only the joined
# form gets the capital.  See the 2026-08-25 block further down.
circus_solution.txt|circus.taf|Congratulations.  You completed the game|SCR_SEED=12 SCR_SKIP_WAITKEY=1
colony_solution.txt|Colony.taf|You scored 200 out of the maximum 200!
cyber_solution.txt|cyber.taf|THE END,or is it?
# 2026-08-25: one line moves with the "The"-prefix fix (the Runner's tense
# has no "the" branch -- see the xfiles block below).
cyber2_solution.txt|cyber2.taf|you have beaton Cyber Warp 2!
# 3.90.  Re-blessed 2026-08-24 for one line: Vluurinik's room description at
# command ~185 goes from "Vluurinik flits around." to "Vluurinik darts in
# circles.".  Not measured in run390 under Wine -- justified from run390's own
# P-code instead, which is stronger here than a single replay would be.
# Vluurinik (NPC 1) has three walks, all with real StartTask/StoppingTask
# pairs, and the line comes from the ChangedDesc of whichever of them is
# "eligible".  run390's viewroom (run390_3.bas:29000, loop at loc_447D1D)
# scans walks ASCENDING, computes its ok flag from StartTask/StoppingTask
# only -- there is NO counter test -- and *overwrites* the NPC's description
# on every eligible walk, so the HIGHEST-numbered eligible walk wins.  That is
# byte-for-byte the same shape as run400's viewroom (Proc_19_63_472CA4, NPC
# loop 4727E2..472931), which four live run400 measurements already confirmed
# this session (FunHouse 0/18, TheCatintheTree, Main Course, Orient Express).
# Scarier used to pick the first walk that was still counting down; it now
# picks the last eligible one, which is what both Runners do.
# ---------------------------------------------------------------------------
# Empty-M1 room alts: a *matching* method-0/1 alt is the starting point even
# when its own M1 is blank, and everything accumulated before it is discarded.
# SCARE used to skip such an alt and keep scanning backwards, which let an
# earlier alt's text survive.  Measured live in run390 under Wine on this game
# 2026-08-24.  Room 7 (Bottom of Bell Tower) has exactly two alts: alt 0 is
# method 2, unconditional, M1 "The end of a rope dangles here."; alt 1 is
# method 1, gated on task 31 (untie rope), with M1 *and* M2 both empty.
#   before "untie rope" (alt 1 not matching), run390 prints
#     "...West returns to the chapel proper.  A huge cracked bell is here.
#      The end of a rope dangles here."
#   after  "untie rope" (alt 1 matching, blank), run390 prints
#     "...West returns to the chapel proper.  A huge cracked bell is here.
#      Also here is the rope."
# -- the rope-dangling line is gone, so the blank matching alt really did reset
# the description to the room's Long ("A huge cracked bell is here." is part of
# that Long, past a <br>, which is why it survives).  See sclibrar.cpp
# lib_find_starting_alt(): run400's lister (Proc_19_63 @472CA4) accumulates
# forwards and applies the display method on a match with no emptiness test at
# all, and the pre-4.0 Runners reach the same place by picking the task alt on
# task-doneness alone (run370 @43318C, run390 @447648 loc_447670) and then
# skipping the base/LastDesc branch.  Only the *non*-matching branch is guarded,
# on M2.  Confirmed independently at all three engine generations: 3.7 (arlo,
# below), 3.9 (here), 4.0 (unravel, below).  19 goldens across 13 games were
# blessed from the old model and were re-blessed for this.
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Re-blessed 2026-08-25 for the pre-4.0 examine/read refusal wording, measured
# on the purpose-built probes p39EXAM.taf (3.90) and p4EXAM.taf (4.00) --
# harness/make_39_examprobe.py and harness/make_400_examprobe.py, driven under
# Wine as Adrift_41/43_p39exam.txt and Adrift_1_p4exam.txt, every command
# echoed.  Pre-4.0 `read <noun that names nothing>` answers the flat, person-
# free "Nothing special." -- `read` is not a verb of its own before 4.0, it is
# ORed into the words that enter examines() (run370 434E2A, run380 43C69D,
# run390 44B7FF), so an unmatched noun falls to the same tail `x` does.  4.0
# gave read its own "You see no such thing."
#
# Line 690 (`read notation`) does not show that sentence, because this game
# ALRs it: the .taf maps [Nothing special] -> [I can tell you nothing about
# that].  The ALR fires only now that scarier finally emits the string the
# Runner emits -- the rewrite is the fix landing, not a second divergence.
# panic_solution.txt below moves the same way, to its own ALR.
# ---------------------------------------------------------------------------
# MEASURED 2026-08-25 on run390 (Adrift_1_cybercow.txt) as the other half of
# the goldilocks event-room fix -- the half where the correct gate prints MORE,
# not less.  `up` out of the well is a ShowRoomDesc task naming Chapel Yard,
# and the Runner prints the day/night event's look text with it:
#
#   up
#   That's a bit of a trick carrying a warm bowl of ... but you manage it.
#
#   Chapel Yard
#   You are at the well.   The rope, which is tied to the well quite securely,
#   leads down. ... Down the hill to the north there is the bus stop.
#   Vluurinik flits around.
#   It is daytime.  You can move north, east, south, west and down.
#
# Scarier printed no "It is daytime." there, because the player was still at
# the bottom of the well -- outside the event's room list -- when the hall
# above was composed.  The same one-line fix supplies it.
#
# The run itself is NOT a clean row and must not be quoted as one: three of the
# 127 commands never reached the Runner (feed[23] `look`, and two later), so
# everything after the first loss is a turn out of step, which is why the
# comparison also shows Vluurinik in the wrong place and the weather event out
# of phase.  Driving this game properly needs, in addition: POPUP_ANSWERS for
# its name and gender InputBoxes (the first two lines of the solution answer
# dialogs, not the game prompt, so they must be stripped from the command
# file), and a `catch fairy` that is spammed rather than typed once -- the
# catch is a random roll, so a fixed feed cannot be replayed turn for turn
# against the Runner without seeding it.
cybercow_win_solution.txt|lair-of-the-cybercow.taf|Thank you for playing Lair of the CyberCow.
cybercow_solution.txt|lair-of-the-cybercow.taf|Your score is 6 out of a maximum of 10.
deaths_solution.txt|deaths.taf|crumbles into dust
donuts_intro_solution.txt|donuts_intro.taf|To be continued (maybe)..
# Measured live in run400 under Wine: the whole 18-command replay matches the
# fixed engine exactly (0 differing commands).  This is the row that pins down
# the precedence half of the walk fix -- run400 lets a higher-numbered walk
# with StartTask 0 shut a lower one down with no counter test at all
# (Proc_19_1_468DA0 @4686FD-468747), even when it has no stops to walk -- and
# the task-state (not counter) test in lib_get_npc_inroom_text().  The other
# half, what a finished 4.0 walk's counter is stamped with, is pinned by
# the_pk_girl instead; see that row.
funhouse_solution.txt|FunHouse.taf|thank you for bravely protecting this important information
gateway_solution.txt|gateway.taf|THE END
hyper_b_s_solution.txt|hyper_b_s.taf|The Flare Rat is dead! Mission complete!
jason_vs_salm_solution.txt|Jason Vs. Salm.taf|Good job then!|SCR_SEED=11
light_up_solution.txt|light_up_4summer_comp.taf|THE END|SCR_SEED=16
# Measured live in run400 under Wine (2026-08-24), full replay, Verbose ON.
# The game is NOT winnable in the real Runner: "Cat sheepishly enters from
# the east." never appears (the cat's walk has expired), so `attack cat` gets
# "I don't understand what you mean!" and the closing `main course` command is
# refused.  Win marker deliberately removed -- the walkthrough is kept for its
# transcript, not for a win.
maincourse_solution.txt|Main Course.taf||SCR_SEED=17
# The 3.9 half of the walk-announcement rewrite was measured on this game --
# run390 under Wine, Adrift_37_melbourne_beach.txt, 2026-08-24.  See the arlo block.
# The same transcript then pinned two more pre-4.0 rules, and this walkthrough
# was re-derived for both (2026-08-25):
#
#   * a non-looping walk with StartTask 0 never runs before 4.0.  Judy's walk
#     is six stops (Kitchen 10, Eating area 10, Den 5, Judy's bedroom 15,
#     follow 5, Outside den 1) and non-looping, and run390 leaves her standing
#     in the Kitchen for the whole game: she is still there at turn 18, and all
#     twenty `give trumpet to judy` typed in her bedroom on turns 36-55 answer
#     with task 17's third restriction, "You can't do that in your present
#     company."  No shift of the walk's start fits both observations, so the
#     walk simply never starts -- the Runner has no game-start seeding, and the
#     ticker's restart-a-spent-walk branch is gated on the walk looping before
#     4.0 (run380 441389, run390 45A585) and unconditional in 4.0.  See
#     npc_start_walk_is_390_noop().  The walkthrough now gives Judy the trumpet
#     and the music in the Kitchen, where she stands, and the two twenty-turn
#     waits are gone; the score is unchanged at 38/41.
#   * a task whose command list holds a bare "*" clears the room refusal.  This
#     game's task 94 is `*` confined to room 0, so run390 answers "I don't
#     understand what you mean!" -- not "You can't do that here!" -- for `play
#     volleyball` and `use shower` typed outside their rooms.  See
#     run_task_has_catchall_command().
#
# With both fixes the 128-command replay is down to 29 differing turns, and
# every one of them is rule 1 (this Wine run had Verbose OFF, so re-entry is
# brief) or RNG: $randwalks picks the NPC enter/exit verb, and `play chess`
# picks a winner.
melbourne_beach_solution.txt|Melbourne Beach.taf|You successfully completed the original game Melbourne Beach
# Measured live in run400 under Wine (2026-08-24).  At command 38, `n` into
# the Dining Car, the Runner prints "The waiter saunters over." -- the same
# line the fixed engine now prints.  The old golden had "Gimme Atip is here.",
# i.e. the plain NPC name rather than the walk's ChangedDesc.  (Unrelated and
# still-open run400 differences in this game: timed-event turn offsets, and one
# spurious "Gimme Atip enters." -- the not-a-room-zero arrival gate.)
#
# The battle narration naming, re-measured off the same transcript 2026-08-25
# and now fixed: run400 fights "the large man" and "BIG BOSS", never "Igotta
# Bigbottom" or "Ivill Getyou".  Both attack procedures name an NPC by
# "<Prefix> <Alias[0]>" when it has a first alias -- Proc_11_1 (the player's
# blow) from any NPC, Proc_11_2 (an NPC's blow) only from one whose current
# attitude is enemy.  "Thug " has no alias and keeps its Name, trailing space
# and all, in the very same transcript.  See battle_print_npc_name() in
# scbattle.cpp; the goldens that moved with it are trabula, shadowpeak (x3),
# cyber2 and light_up.  The walk
# direction suffixes such as "wobbles in from the east" were the 4.0 half of
# the walk-announcement rewrite; see the arlo block for the full measurement.
orient_express_solution.txt|Orient_Express.taf|You successfully complete your assignment.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
#
# Re-blessed again 2026-08-25 for the event start-turn tick.  Measured in
# run400 under Wine, transcript Adrift_36_orient_express.txt, 53 commands, all
# 53 echoed.  Turn 43 `use phone' starts event 2 [Phone rings] (Time1 = 1,
# Time2 = 8, PrefTime1 = 2, one Where room) and the Runner prints its StartText
# *and* its PrefText1 on that single turn; turn 46 `give card to habibo' does
# the same with event 3 [Driveby Shooting] (PrefTime1 = 3).  We printed
# neither: our start turn consumed no tick, so the first pref-time comparison
# only happened a turn later, by which time the player had left the room and
# evt_can_see_event() was false.  checkevent() is one straight run of state
# tests -- start and tick happen in the same call, and the task-started path
# adds 1 to the roll so the end time still comes out at start + roll.  See
# scevents.cpp's ES_AWAITING branch and the WINE-TRANSCRIPTS-TODO entry.
#
# What is left in that replay, at SCR_SEED=424242: turns 21, 22 and 37-41 are
# rule 1 (this Wine run had Verbose OFF, so re-entry is one brief line), and
# turn 52 is the harness eating the Runner's "[Press any key to end]".  The
# train-stop event texts move with the seed -- event 0 rolls 10..19 and events
# 1..3 roll 1..7 -- so their turn numbers are RNG, not an engine difference.
screen_savers_solution.txt|The Screen Savers On Planet X.taf|You've managed to get everyone to the set!
secret_of_lost_world_solution.txt|SecretOfLostWorld.taf|The ship is slowly sailing away
space_boy_solution.txt|Space Boy's First Adventure.taf|STAY TUNED FOR MORE EXCITING EPISODES
sun_empire_solution.txt|Sun_Empire_Quest_For_The_Founders.taf|You scored 135 out of the maximum 145!
tcom_solution.txt|tcom.taf|the file entitled "tcom2"
think2_solution.txt|Theannihilationofthink2.taf|Think.com has been restored
toxically_earth_solution.txt|Toxically_Earth.taf|Thanks for playing RON: TOXICALLY EARTH
# Measured live in run400 under Wine (2026-08-24).  Two findings:
#
#  * Room alts with Var3 = 0.  The Lobby and Davis Storage Warehouse each
#    carry a type-2 alt with Var2 = 4 ("isn't in the same room as") and no
#    object selected.  run400 prints them on every visit -- "Do you have your
#    badge?" and "You left the key back in D.C., didn't you?" -- because it
#    runs the test against an object it cannot find.  SCARE printed neither,
#    ever.  Fixed in sclibrar.cpp lib_use_room_alt(); only three alts in the
#    whole corpus use Var3 = 0 (these two and one empty one in House.taf).
#
#  * The walk line ("You notice a man sitting alone in a dark car." beside
#    "Langly is here.") could NOT be measured: run400 refuses this
#    walkthrough's `burn memo`, `take knife`, `knock`, `look at camera`,
#    `get in the van` and `take directions`, so by command 53 it is back in
#    the FBI parking garage while we are in Bellefleur.  Those are
#    task-matching divergences, a separate investigation.  The line is
#    corroborated internally instead: the very next command's own game text
#    is "The man in the dark car watches you silently as you climb in the
#    van.", so the man is in that room and the description should say so.
# Two engine fixes land on this row, both measured live in run400 under Wine
# 2026-08-24; afterwards the replay is Runner-exact.
#  1. Room-alt player conditions "is/isn't holding" are the Runner's *recursive*
#     possession predicate (run400 4579C1/4579EB call Proc_21_46 @44615C), so an
#     object inside or on something carried or worn counts as held; SCARE tested
#     the object's own position only.  Parking Garage B (room 3) carries a
#     Var2=1, Var3=2 alt on the gun, which lives inside the worn holster, so
#     run400 prints "It may be unwise to pull a gun on this guy." on every visit
#     where SCARE printed the unconditioned alt instead.  This game is the only
#     row in the corpus the fix moves.
#  2. The empty-M1 room-alt start rule (see the lair-of-the-cybercow rows).
#  3. 2026-08-25 -- A "The" PREFIX KEEPS ITS CAPITAL.  The Memo's Prefix is
#     literally "The", and run400 prints it back that way everywhere in
#     Adrift_22_xfiles.txt: "... Your Badge and The Memo from Your Desk.",
#     "Your Coffee Mug and The Memo are on Your Desk", and "I don't understand
#     what you want me to do with The Memo."  The Runner's normalizer, tense
#     (Proc_21_13_44F474 @44F474, reached from the name builder
#     Proc_21_31_448710 in its normalizing mode 0, and handed Prefix & " " &
#     Short as one string), tests exactly six things -- the whole string
#     against "a"/"an"/"some" and its head against "a "/"an "/"some " -- and
#     returns its argument untouched otherwise.  There is no "the" branch, in
#     4.0 or in the byte-identical pre-3.9 tense (run370 @420F28, run380
#     @425FA8).  scarier's lib_print_object_np() had one, and all it ever did
#     was lower-case the author's capital: the fix is its deletion, since
#     falling through prints the prefix verbatim anyway.  Two lines move here
#     (cmd 17's "You take The Warehouse Key from Case File 10193." among them);
#     cyber2, afdfr and the 3.90 spirits_flight move too.
#  4. 2026-08-25 -- ON BEFORE IN, AND IN ONE SENTENCE.  `x desk` answers
#     "Your Desk is open.  Your Coffee Mug and The Memo are on Your Desk, and
#     inside is Gun Holster, Your Cell Phone, Neatly Wrapped Gift and Your
#     Badge." (Adrift_22_xfiles.txt line 9); scarier put the container first
#     and ended the sentence between the two clauses.  The Runner lists both
#     in one helper, whatisinon (Proc_19_26_46A950 @46A950), whose second
#     argument is a mode -- the "on" half is guarded on mode <> 0 (loc_46A083)
#     and the "in" half on mode <> 1 (loc_46A41E).  examines() and inventory()
#     pass 2 and get both; openclose() and the room lister pass 0 and get
#     containers only, which is why `open desk` one command earlier already
#     agreed and only the examine path diverged.  When the "on" half printed
#     anything it sets var_9E, and the "in" half tests that FIRST (loc_46A786):
#     ", and inside is " plus the plain list, no container name, no new
#     sentence, and the single closing '.' comes once at loc_46A8C6.  Note the
#     count-1 and count-2 "<a> is inside <cont>" branches are both guarded on
#     var_9E = 0, so a surface listing forces the joined wording whatever the
#     count -- the `If var_9E = 1` arms nested inside them (loc_46A4F1,
#     loc_46A66C) are unreachable.  That literal reading is what is
#     implemented, and it is what moves ADRIFTMAS_Party (in-count 2) and
#     yonastoundingcastle (in-count 1) as well as this row; a live probe of
#     those two counts is still wanted.  Pre-3.9 is excluded: run380 has no
#     combined lister at all (whatisin1 @4297AC and whatisin2 @42998C are
#     separate, and examines @43D5EC prints "  Inside <obj>" or "  On <obj>",
#     never both), and ", and inside is " is absent from run370.exe and
#     run380.exe.
#  5. 2026-08-25 -- THE SECOND REPLAY AGREES ON EVERYTHING ELSE.
#     Adrift_31_xfiles.txt is a separate 20-command run of the same game and
#     the same Runner.  Its feed is CRLF, so an empty Return followed every
#     command and each printed a bare "Nope!" (see the bare-Return section in
#     the notes) -- which shows up as a constant one-turn shift, not as noise.
#     Realign run400 turn N (minus its trailing " Nope!") against scarier turn
#     N+1 and 19 of the 20 turns are byte-identical; the 20th is `burn memo`.
#     This is the run that says the four rules above hold outside the one
#     transcript they were read from.
# Re-blessed 2026-08-25, three lines (`burn memo`, and the two score lines it
# moves), for the 4.0 `%object%` case rule.  run400 substitutes an object's
# Short (or an Alias) into a `%object%` task command VERBATIM and compares the
# result to the lower-cased input -- Proc_19_37_458E6C, mdlSpreadTheLoad.bas
# loc_458BBC-loc_458E69, which has no LCase() on the substituted name, unlike
# its `%character%` twin at loc_46918C which lowers the Name and every Alias.
# So a capitalised Short can never bind, and xfiles' objects are all
# capitalised ("Memo", "Coffee Mug", "Gun Holster"): task 24, `Burn %object%`,
# is dead code in the real Runner.  Measured on the synthetic p4BURN.taf over
# four Wine rounds (harness/make_400_burnprobe.py carries the cell table):
# thirteen restriction shapes all AGREED, Repeatable OFF agreed, and then
# `pa %object%` + Short "Widget" refused `pa widget` AND `pa Widget`, while
# `PX %object%` + Short "coin" took both `px coin` and `PX coin`.  No article,
# no Prefix and no partial name binds either.  Ported as
# uip_compare_reference_strict() in scparser.cpp, gated on TAF_VERSION_400 and
# on the reference being an object.  64 of the 432 corpus .taf are 4.0 games
# with `%object%` in a task command; this moved one golden line, and it moved
# it onto what Adrift_22_xfiles.txt:17-18 and Adrift_31_xfiles.txt:32-33 both
# print.  Score 296 -> 295, "3 points short" -> "4 points short".
# `look up byers` (Adrift_22_xfiles.txt:232) is NOT a divergence: the
# `%character%` matcher gates on the NPC's seen byte (npc.global_26) and the
# Runner had not met the Lone Gunmen yet at the parking garage, where this
# golden reaches the line long after Byers has been listed.  Porting the NPC
# seen gate is still open.
# Re-blessed 2026-08-25, one line (589, `open phone book`): no Runner has ever
# printed "Open what?".  scrunner.cpp's `open *` row was asymmetric with the
# `close *` row directly beneath it, and now routes to lib_cmd_open_other() for
# the Runner's "You can't open that."  Measured on both probes (run390
# Adrift_41/43_p39exam.txt, run400 Adrift_1_p4exam.txt: bare `open` and `open
# door` answer "You can't open that." in both), and confirmed a third way by
# panic.taf's exhaustive ALR Originals table, which lists thirty-three
# "<Verb> what?" library messages and neither "Open what?" nor "Close what?".
# This line is still not what run400 prints -- run400 matches `phone` against
# "Your Cell Phone" and answers "Your Cell Phone is already open!"
# (Adrift_22_xfiles.txt lines 226-231), which is the matcher gap logged above --
# but the library half of it is now the Runner's.
xfiles_solution.txt|The_X-Files_A_New_Beginning.taf|Welcome to the Resistance.
del_sol_solution.txt|Del Sol.taf|Your score is 26 out of a maximum of 46.
inverness_solution.txt|inverness.taf|Your score is 75 out of a maximum of 205.
les_feux_solution.txt|Les Feux de l'enfer.taf|Votre score est 25 sur un maximum de 115.|SCR_SEED=138 SCR_SKIP_WAITKEY=1
lifesimulation_solution.txt|lifesimulation.taf|Your score is 0 out of a maximum of 0.
matts_house_solution.txt|Matt's House.taf|Your score is 5 out of a maximum of 5.
mr_smith_solution.txt|The_Search_For_Mr_Smith.taf|You scored 90 out of the maximum 100!
phoenix_destiny_solution.txt|Phoenix_Destiny.taf|Gold: 100
questi_solution.txt|QuestI.taf|Your score is 10 out of a maximum of 10.
shadow_of_the_past_solution.txt|Shadow_Of_The_Past.taf|You now realize that the statue was you from a past life.
# 3.90.  2026-08-25: nine lines move with the "The"-prefix fix -- The Spirit
# Dagger, The Orb of Storms and The Amber of Flames all carry a "The" Prefix.
# The run390 decompilation does not reach its normalizer, so 3.9 is bracketed
# by the measured 3.8 and 4.0 behaviour rather than read.  See the xfiles block.
spirits_flight_solution.txt|The_Spirits_Flight.taf|Your score is 50 out of a maximum of 95.
srsintro_solution.txt|SRSintro.taf|
the_nonsense_machine_6000_solution.txt|The_Nonsense_Machine_6000.taf|
# Marker 27 -> 26 on 2026-08-24 with the 4.0 output filter (see the humbug
# row): the end-of-game summary's turn counter is frozen when the winning task
# completes, one event tick before the flush would have read it.
the_town_of_azra_solution.txt|The_Town_Of_Azra.taf|Number of turns passed: 26
# Azra ships as two files and they are NOT the same game to play.  The
# underscored IF Archive build is a 4.00-signature upconversion of the author's
# 3.90 original: the ADRIFT 4 editor left every battle attribute degenerate, so
# accuracy 0 is never > agility 0, every blow misses and combat is an eternal
# stalemate (verified live in run400 -- adrift-combat-zero-accuracy-stalemate).
# Combat is Azra's only income, so that build caps at the shops and the inn,
# which is all the 27-turn row above can reach.  The spaced adrift.co build is
# the untouched 3.90 file, gets battle_legacy (strength - defence, every blow
# connects), and plays five of the six goals the author lists in the intro:
# kill a bandit, sell a deer carcass to Drako, buy from all three shops, stay at
# Gralle's Inn, learn Stealth Tactics.
#   RE-DERIVED 2026-08-24 (505 -> 58 turns) for the dead-NPC port.  Azra's
# economy assumes a battle-killed NPC respawns on the next lap of its patrol
# walk -- tasks 19 (#banditkristdies) and 37 (#deerdies) both hide their NPC
# and hand back stamina -- and it never did in the real Runner: killchar
# (run390 @42D410, run400 @44B13C) stamps room = &HFB (-5) after running the
# KilledTask, and the walk ticker breaks out of the walk loop at -5 (run390
# loc_45A4BC, run400 loc_4685B6).  The ADRIFT 4 manual agrees in prose (l.2659,
# "the character to disappear").  So the old route's fifteen carcass sales were
# bought with an engine bug, and GOAL 5 (a $7500 house) is unreachable: one
# bandit purse + one carcass tops out at $959.68, and Stealth costs $800.  See
# The_Town_Of_Azra_walkthrough.md.
the_town_of_azra_v390_solution.txt|The Town Of Azra.taf|Number of turns passed: 58|SCR_SEED=26 SCR_SKIP_WAITKEY=1
thetest_solution.txt|thetest.taf|Your score is 5 out of a maximum of 25.|SCR_SKIP_WAITKEY=1
# thetest IS winnable (2026-08-01, verified live in run390 to "Well done!  You
# won!" at 20/25): the colour-door needs addything==3, i.e. two consecutive
# key/door colour matches on `unlock door` (the old "circular lock /
# unwinnable" verdict misindexed task 15's variable restriction -- var1-2 is
# addything, not robot2).  The route's unlock/shout spam is RNG-timing under
# the fixed seed; see thetest_walkthrough.md for the mechanism.
thetest_win_solution.txt|thetest.taf|Well done!  You won!|SCR_SKIP_WAITKEY=1
through_time_solution.txt|Through time.taf|This is as far as this adventure will take you at this point.
to_hell_and_beyond_solution.txt|To_Hell_And_Beyond.taf|You have entered the town of Oran.
# The assisted To-Hell row needs BOTH aids: the game's combat data is all-zero
# accuracy/agility AND its mid-game progression moves have an unset "To:" combo
# (Var2=-1).  With only SCR_ASSUME_COMBAT the player never leaves the mansion
# and the closing "claim the throne" is not understood (the 2026-07-14 "desync"
# was exactly that -- a replay missing SCR_ASSUME_MOVES).
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
to_hell_and_beyond_assisted_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1 SCR_SKIP_WAITKEY=1
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
to_hell_and_beyond_assisted_max_solution.txt|To_Hell_And_Beyond.taf|You are now ruler of Beyond|SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1 SCR_SKIP_WAITKEY=1
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
argh_solution.txt|ARGH_sGreatEscape.taf|You scored 98 out of the maximum 125!
spam_solution.txt|SPAM.taf|Spam King|SCR_SKIP_WAITKEY=1
wreckage_solution.txt|Wreckage.taf|you've rescued yourself
vagabond_solution.txt|Vagabond.taf|The End|SCR_SKIP_WAITKEY=1
woof_solution.txt|Woof.taf|I'm back.
undefined_solution.txt|Undefined1.taf|An end is defined.
ecod3_solution.txt|ECOD3.taf|In an alley behind Denny's.
goblinhunt_solution.txt|goblinhunt.taf|Tomorrow is the next goblin hunt.|SCR_SKIP_WAITKEY=1
agent4f_solution.txt|agent_4F[1].A.taf|You wake with a start.  What a terrible dream!
invasion_shirts_solution.txt|Invasion of the Second-Hand Shirts.taf|You're floating through the air above the trees.
adriftorama_solution.txt|adriftorama.taf|*****You Win!*****|SCR_SEED=18 SCR_SKIP_WAITKEY=1
# The seventeen games swept out of the Key & Compass ADRIFT index (2026-08-02);
# see the per-game notes/*_walkthrough.md for where each .taf came from.
wax_worx_solution.txt|wax_worx.taf|[PRESS ANY KEY TO DIE]
sommeril_solution.txt|sommeril.taf|www.angelfire.com/games5/sommeril
dragonshrine_solution.txt|DragonShrineR43.taf|ended the Curse of Dragon Shrine|SCR_SKIP_WAITKEY=1
shardsofmemory_solution.txt|shardsofmemory.taf|My adventure has ended, and in victory besides|SCR_SKIP_WAITKEY=1
TheADRIFTProject_solution.txt|TheADRIFTProject.taf|the entire ADRIFT community greet you|SCR_SKIP_WAITKEY=1
ShadricksUnderground_solution.txt|ShadricksUnderground.taf|the robbers were caught red handed in the vault|SCR_SKIP_WAITKEY=1
ticket_solution.txt|ticket.taf|You won and managed to score 110 out of a possible 110|SCR_SEED=10 SCR_SKIP_WAITKEY=1
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
cleft_solution.txt|cleft.taf|You scored 100 out of the maximum 100!
Tear_solution.txt|Tear.taf|Suddenly the world seems a brighter place, and you feel there is a good
tq3_solution.txt|tq3.taf|Please forward your comments to chris@jons.org.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
# Re-blessed 2026-08-25, four lines: a room with no description of its own says
# "There is nothing of interest here." in 3.8 and 3.9.  run390 appends it while
# rendering, only when the branch's own alternative text is empty AND the Long
# is empty (4478CA, and the same guarded shape at 4479A5/447A3A/447ACF, each
# preceded by a branch that takes the LastDesc instead and jumps clean past it);
# run380 substitutes it into the empty Long at LOAD (447FEE) and arrives at the
# same output.  3.7 has no such string; 4.0 dropped it.  Read the guard, not the
# literal -- hanging it off an empty Long alone moves sixteen goldens, gating it
# on "nothing has described this room yet" moves exactly two, this one and
# richard_solution.txt, both 3.90.  Measured on p39EXAM.taf's third room, an
# empty Long with no alts and no objects: Adrift_41/43_p39exam.txt answer
# "There is nothing of interest here.  You can only move west." to both `e` and
# `look`, while the 4.00 twin (Adrift_1_p4exam.txt) prints the exits alone.
yeh_solution.txt|yeh.taf|Your score is 3100 out of a maximum of 3400.
# Re-blessed 2026-08-25 for the on-before-in joined listing (see item 4 in the
# xfiles_solution.txt block above): the wardrobe is both a surface and an open
# container, so `x wardrobe` now reads "The suitcase is on the wardrobe, and
# inside is a leather jacket and an assortment of shoes."  This is one of the
# two rows that exercise run400's unreachable count-1/count-2 arms -- here the
# in-count is 2 -- so it is the literal disassembly that is blessed, not a
# measurement.
ADRIFTMAS_Party_solution.txt|ADRIFTMAS_Party.taf|"Merry ADRIFTMAS TO ALL!  And to all a good night!"|SCR_SKIP_WAITKEY=1
Glum_Fiddle_solution.txt|Glum Fiddle.taf|Your score:100 out of 100.|SCR_SKIP_WAITKEY=1
JGrim_solution.txt|JGrim1.0.taf|WHOOOOOSH|SCR_SKIP_WAITKEY=1
mysteryofcaves_solution.txt|mysteryofcaves.taf|Your finishing rank is: Godlike Adventurer.|SCR_SKIP_WAITKEY=1
chooseyourown_solution.txt|chooseyourown.taf|"A hunch," you say. You link arms with Sharon Elson.|SCR_SKIP_WAITKEY=1
fantasyworld_solution.txt|fantasyworld.taf|You scored 0 out of the maximum 500!
# Grumble's arrival is missing from 12 turns of these two goldens ON PURPOSE:
# sa.taf carries 65 ALRs whose Original spans the two-space join and deletes
# the sentence at a named spot ('quiet.  Grumble complaining of beer
# deprivation staggers in from the west.' -> 'quiet.').  Bisected live in Wine
# and re-derived with harness/make_400_walkalrprobe.py; see the 2026-08-25
# block further down.  Never "fix" a missing walker line here without checking
# the ALR list first.
# The comp release sophie.taf carries one ALR the author later deleted: #418
# [, and] -> [:].  ", and" is a very common string, so it wrecks eight
# sentences, and it is the ONLY difference between these two goldens over the
# stretch they share -- sophie_comp reads "near the walls: a light fitting"
# where sophie reads "near the walls, and a light fitting".  Scarier applies
# it, and that is right.  Do not "fix" it:
#   * the author's own two fixup rules, #455 [of the moment: throw yourself at
#     Smunch.] -> [of the moment and throw yourself at Smunch.] and #458
#     [inventory: so on] -> [inventory, and so], have a COLON in their
#     Originals where the raw game text has ", and".  He can only have seen
#     those colons in a Runner, so run400 does apply #418 -- and applies it
#     BEFORE the two longer fixups get their turn, which is exactly what 4.0's
#     repeat-until-stable pass buys (a single length-descending pass would run
#     the fixups first and leave them dead).
#   * sa.taf, the later author release, DELETES #418 and keeps #455/#458, now
#     dead rules: the cleanup of a mistake.
# 2026-08-25: the Wine transcripts named Adrift_41..46_sophie.txt do NOT
# contradict this.  Every sophie run under run400 used sa.taf or one of its
# doctored saF* variants -- sophie.taf has never been run -- so the ", and"s
# that survive in them are sa.taf's untouched text, and they agree with
# sophie_solution.expected.txt line for line.  The transcripts are named after
# the GAME, not the .taf; check the row below before quoting one.
sophie_solution.txt|sa.taf|You have won.|SCR_SKIP_WAITKEY=1
sophie_comp_solution.txt|sophie.taf|You have won.|SCR_SKIP_WAITKEY=1
cursed_solution.txt|cursed.taf|The honour will be all mine, father|SCR_SKIP_WAITKEY=1
easter_solution.txt|easter.taf|***You have won***|
# Re-blessed 2026-08-25 for the same on-before-in joined listing, with an
# in-count of 1: "Ye olde desk clutter is on ye alchymist's desk, and inside is
# ye magic crystal."
yonastoundingcastle_solution.txt|yonastoundingcastle.taf|Incredible victory!|SCR_SKIP_WAITKEY=1
# The twenty-one entries of the 1st, 2nd and 3rd ADRIFT One-Hour Game
# Competitions (2003), swept in on 2026-08-03 -- see the per-game
# notes/*_walkthrough.md for where each .taf came from.
# Several of these are deliberately unwinnable or end in the player's death;
# the marker is the game's own final line in each case, not a victory string.
# 1st One-Hour Game Competition
frog_solution.txt|frog.taf|So you hop away with your fairy princess, to live hoppily ever after.
chicken_solution.txt|chicken.taf|That was the last time either of you threw a brick at something.
endgame_solution.txt|endgame.taf|Really really.
# Wine 2026-08-23, run400, Adrift_16/17_hauntedhouse.txt.  The feed was
# haunted.taf's solution driven at hauntedhouse.taf -- a mispairing (see
# notes/WINE-TRANSCRIPTS-TODO.md) -- but both engines were fed the same
# nonsense, so the turns still compare: 115 of the 116 commands echoed, the
# first loss at feed[43] "w".  Before that loss, two divergences, and one of
# them is an engine bug we now fix:
#   turn 34 "melt statue" from the Front porch, statue in the Entrance --
#     run400 "You can't see the statue.", scarier the game's DontUnderstand.
#     therest() resolves the noun before it dispatches the verb; ported to
#     lib_cmd_verb_object().
#   turn 3 "open door", no door object in the game at all -- run400 "You
#     can't open that.", scarier "Open what?".  CLOSED by the p39EXAM/p4EXAM
#     probes: bare "open" and "open door" both answer "You can't open that."
#     on run390 AND run400, so it was a one-line asymmetry with the "close *"
#     row beneath it, not a version split.  No Runner has ever said "Open
#     what?".
# The brief re-entry headings at turns 21/27/41 are rule 1, not an engine
# difference: this run predates measure.sh, so Verbose was OFF.
# RE-DRIVEN CORRECTLY 2026-08-25, run400, Adrift_1_hauntedhouse.txt: this
# game's OWN 42-command solution, Verbose ON via measure.sh, all 42 echoed.
# compare_wine_transcript.py reports ONE differing turn, the last, and the
# whole of the difference is the Runner's `[Press any key to end]` tail --
# which scarier emits as a SCR_TAG_WAITKEY pause rather than as text, by
# design (os_ansi.cpp).  41 of 42 turns are identical text; the remaining
# blank-line differences the raw files show are the transcript writer's, not
# the engine's (the Runner writes a room block as consecutive lines with no
# blank between them, and blank lines only between turns).  So this row is
# now measured clean end to end, and the mispaired Adrift_16/17 run above is
# superseded except for the two engine bugs it found.
hauntedhouse_solution.txt|hauntedhouse.taf|you congraulate yourself on a job well done.
microbe_willie_solution.txt|microbe_willie.taf|pestilence (basically, more of your kind) throughout the world.
amonkeytoomany_solution.txt|amonkeytoomany.taf|Hooray! You've made it through the game!
# 2nd One-Hour Game Competition
dfu_solution.txt|DFU.taf|Thank you, and good night.
percy_solution.txt|Percy.taf|prince among vikings
forum_solution.txt|forum.taf|You Won!|SCR_SKIP_WAITKEY=1
# 3rd One-Hour Game Competition
cbn_solution.txt|CBN.taf|you excelled yourself|SCR_SKIP_WAITKEY=1
cbn2_solution.txt|cbn2.taf|the archives room goes up in flames|SCR_SKIP_WAITKEY=1
crm_solution.txt|CRM.taf|You take a long bow as the curtains close for the show, and the dead body
ecod2_solution.txt|ECOD2.taf|has been captured|SCR_SKIP_WAITKEY=1
imagination_solution.txt|Imagination.taf|Was this all just in your imagination?
asdfa_solution.txt|asdfa.taf|bottle of Nightmare Inducer fluid back in his pocket|SCR_SKIP_WAITKEY=1
demonhunter_solution.txt|demonhunter.taf|journey to the beginning of your new life. You're a demonhunter.
forum2_solution.txt|forum2.taf|***You have won!***|SCR_SKIP_WAITKEY=1
pyramid_solution.txt|pyramid.taf|moves out of your way allowing you to make a hasty retreat.|SCR_SKIP_WAITKEY=1
saffire_solution.txt|saffire.taf|you reach heaven
shore_solution.txt|shore.taf|an island shrouded in a steel fog.
ticktick_solution.txt|ticktick.taf|I'm afraid you are dead!
# The `downloaded/` sweep (2026-08-03): games whose upstream walkthrough was
# harvested off IFDB into test/adrift4/downloaded/ and whose .taf was
# already on this machine.  See downloaded/INDEX.md for the provenance of each
# walkthrough and the per-game notes/*_walkthrough.md.
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
# Measured live in run400 under Wine (2026-08-24): the replay desyncs, so the
# two changed lines (the Kitchen's "The small brown terrier is wandering
# around." vs "The small brown dog is here.") could NOT be measured.  run400's
# Dog is in different rooms from command 31 onward -- and it was already in
# different rooms under the OLD golden, so that is pre-existing and unrelated
# to the walk fix.  run400 also refuses the ending ("You do not have the keys
# so it is probably not a good idea to go just yet."), so the game does not
# finish there.  Two engine-wide divergences this replay exposed, both also
# visible in xfiles: run400 lower-cases object state names ("switched off",
# "switch in the on position") where we capitalised them -- FIXED and
# re-blessed 2026-08-25, three lines here (two "in the On position" -> "in the
# on position", one "switched Off." -> "switched off."), each of which the
# transcript itself prints at Adrift_23_where_are_my_keys.txt lines 34, 50 and
# 64.  The rule is narrower than "run400 lower-cases states": ONLY
# %state_<obj>% is folded, and it is folded whole
# ("R1" -> "r1", "In the UP position" -> "in the up position", mid-sentence
# too), while the examine lister and %obstate% print the States entry
# verbatim.  Measured on the synthetic p4STATE.taf, six objects x four readers
# x two states, Adrift_1_p4state.txt, all 29 commands echoed -- the corpus's
# destructive shapes ("Facing South", "Sur la gauche", "Locked Off", "R1")
# were chosen precisely so a whole-string fold could be told apart from a
# first-letter one.  Ported in scvars.cpp's state_ branch alone.
# -- and %in_<obj>% / %on_<obj>% pick their
# listing format by content count, exactly as the library listers already do.
# The latter is FIXED (var_use_alternate_format() in scvars.cpp).  Measured in
# Adrift_23_where_are_my_keys.txt: `open fridge` (CompleteText "...%in_fridge%", three objects
# inside) prints "Inside the fridge is a tub of butter, a butter knife and a
# bottle of milk.", while the two-object control `open unit` in the same
# transcript keeps the postfixed form, "A large knife and a jar of coffee are
# inside the kitchen unit."  Same 1/2/3+ selector as run400's single lister at
# 0006A418; before TAF_VERSION_390 only the prefixed form exists.
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
lost_solution.txt|LOST.TAF|place your foot on the path leading up the crumbling cliff|SCR_SKIP_WAITKEY=1
lost_down_solution.txt|LOST.TAF|has shown you a doorway back to that brighter world.|SCR_SKIP_WAITKEY=1
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
# Confirmed the empty-M1 room-alt start rule (see the lair-of-the-cybercow rows)
# at 4.0 as well: measured live in run390's sibling run400 under Wine 2026-08-24
# (pfx/drive_c/adrift/Adrift_34_unraveling_god.txt).  Every "Outside the MagLab" description in
# that transcript ends at "...The lot where you always park is to the south."
# and is never followed by the "As nice of a day as it is, though, ..." block
# the old golden carried -- a later matching method-0/1 alt with a blank M1
# discards it.  Driving note: this game's two opening "(press any key)" pauses
# need TYPE_SLEEP=0.6 ENTER_SLEEP=1.6, or the keystroke lands before the pause
# exists and the pause then eats the following real command.
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
# the Hangover entry in notes/WALKTHROUGH_TODO.md for that divergence.
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
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
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
dancing_even_him_solution.txt|dancingevenhim.taf|it is an anagram of Vending Machine|SCR_SKIP_WAITKEY=1
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
enquete_a_hauts_risques_solution.txt|EnqueteAHautsRisques.taf|Votre score est de 59 sur un maximum de 59!
# Shadrick's Travels (Mystery) -- WIN, 100/100, and the whole game has exactly
# four scoring actions (20 + 20 + 10 + 50), all of which this route fires.  The
# upstream file is a session transcript with a CP1252 0xD8 as its prompt glyph,
# so the commands are the lines starting with that byte; 22 of them, replayed
# verbatim including the author's three duds (`x wood` and `climb tree` both
# hit the disambiguator, and `tire swing to tree` is a typo for `tie`).  They
# are kept because ADRIFT's "Please be more clear" does NOT consume the next
# line, so they cost nothing and the transcript stays faithful to the source.
shadricks_travels_solution.txt|ShadricksTravels.taf|You scored 100 out of the maximum 100!
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
monsters_solution.txt|Monsters_r2.taf|You scored 40 out of the maximum 40!
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
# 3.80.  Re-blessed 2026-08-24 with the other eighteen pre-3.9 rows; the
# five measured wording rules are written up above the akron row below.
# Re-blessed 2026-08-25 for the loader's whitespace trims.  run400's object
# loader strips every TRAILING space off Prefix (mdlSpreadTheLoad.bas
# loc_490100..loc_49015C) and every LEADING space off Short
# (loc_490170..loc_4901CC); run370 @43F5DA and run380 @4481B2 do the same.
# Fourteen objects across nine corpus games are written with the spaces still
# on, and each printed a double space where the name gets joined -- here
# marooned's obj 15 " trash" in "a pile of  trash", Crime_Adventure's obj 17/20
# "a " in "a  cookery book", first.taf's obj 9/10/12 "fresh "/"old " in "fresh
# bread and fresh  turkey", and hhorror's obj 51 " floorboards".  The trim now
# runs at parse time, in parse_trim_object_names(), because in the Runner it
# happens in the loader and the noun matcher therefore sees it too.
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
# 3.80.  Re-blessed 2026-08-24 with the other eighteen pre-3.9 rows; see the
# comment block above the akron row below.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
wrecked_solution.txt|wrecked.taf|Hope you enjoyed playing Wrecked.|SCR_SEED=234
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
largo_winch_solution.txt|largo-winch.taf|Votre score est de 97 sur un maximum de 97!
# Three Monkeys, One Cage (Robert Goodwin, 2003) -- WIN, 98/100, and 98 is the
# ceiling: every one of the game's 23 scoring actions is banked.  The author
# wrote a `# jump out` chain whose +2 sits AFTER the two Execute-Task actions
# that end the game.  That +2 IS credited -- both here and in run400, which was
# probed for exactly this in 2026-08-09 (RUNNER_TESTS_TODO.md section 4, the
# `EG` arena probe) -- but the game is over by then and this game's score is an
# author variable that only `score` prints, so nothing can ever show it again.
# 98 is the ceiling of what a player can SEE, and that is the game's own
# authoring bug, not ours.
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
# Re-blessed 2026-08-24 for the 4.0 output filter (see the humbug row below).
# This is the game that decided it: its "chimp" task prints the ALR original
# [CHIMPSIGNAL=%signal_to_chimp%] and only afterwards increments the variable,
# so the freeze at the nested task's completion leaves the raw token on screen
# and shifts every later signal message one back.  run400 does exactly that --
# measured on the game (Adrift_16.txt), not inferred from the probes.
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
# Re-blessed 2026-08-24 for the walk rewrite (scnpcs.cpp npc_tick_npc /
# npc_walk_is_enabled).  Every changed line here is a walk ChangedDesc pick,
# and each one moved *towards* the game's own sense: Grandad now "is sitting
# at one of the tables" in the pub instead of snoring in his armchair, and
# "is sleeping beneath an ultraviolet light" once he has been captured;
# Dennis "lies unconscious on the ground" after he is knocked out.  The old
# golden repeated one armchair line everywhere.
#   Justified from run400's own room lister, Proc_19_63_472CA4, NPC loop
#   4727E2..472931 (read 2026-08-24).  Its per-walk "ok" is built from the
#   task fields alone -- 47283C StartTask == 0 -> ok, 472867 StartTask done
#   -> ok, 472893 StoppingTask done -> not ok -- with no test at all against
#   the walk's counter, and the pick at 4728B0 is
#       If ok = 1 And walk.ChangedDesc <> "" Then desc(npc) = walk.ChangedDesc
#   inside an *ascending* loop, so the highest-numbered enabled walk with a
#   non-empty description wins and a spent walk goes on describing its NPC.
#   The empty-string guard is why the fairy keeps her own walk text rather
#   than being blanked; the reason her golden line lost "She doesn't look
#   very happy." is the other half -- that sentence is her InRoomText, and
#   NPC 25's single walk (StartTask 254, no StoppingTask, ChangedDesc "A
#   fairy sits on a pile of junk nearby.") now covers it for good once task
#   254 is done.
# Re-blessed again 2026-08-24 for two NPC/container wording fixes measured in
# run400 under Wine (sclibrar.cpp lib_list_in_object_normal and
# lib_list_npc_inventory):
#   * "On the triangular table is some swimming goggles, ..." -- the surface
#     and container listers say " is " unconditionally, never " are ".
#   * "Grandad is wearing a hat, and carrying a document." -- the carried
#     clause drops the subject *and* the second "is" in 3.9/4.0.
# The two remaining changed lines (the Tunnel's "Grandad stands nearby." and
# "Grandad walks to the south.") are the NPC_WALK_EXPIRED = -1 sentinel, and
# are NOT measured here: humbug randomises three secrets at start-up (the dial
# combination, the magic word and the keypad code), so a command-for-command
# Wine replay diverges at the keypad -- see notes/WINE-TRANSCRIPTS-TODO.md,
# "How humbug was made replayable".  The sentinel rests on the_pk_girl's
# measurement and on run400's own P-code, where the walk counter's reset value
# is `push &HFF 'Byte` = -1 (VB Decompiler prints a signed byte; the same
# opcode/operand at 00068805 is the Step of a For ... To 0 loop).
# Still a win: the marker below is unaffected.
# Re-blessed 2026-08-24 with thirteen other rows for the removal of the
# bracketed pronoun echo (scrunner.cpp).  Upstream SCARE answered a command it
# had rewritten with an italic "[Drop a paper aeroplane]" line; no Runner does.
# Measured live here on run400 -- "Drop it" gets a bare "Okay.  I have dropped
# the paper aeroplane." -- and confirmed against the other three by string
# search: run370 and run380 contain no "[" literal at all, and run390's only
# one is the "[More]" pager.  94 lines went, in adrift_maze, archie, cellar,
# cruel, humbug, iqsfot, man_overboard, provenance, shred_em,
# TheADRIFTProject, veteran, wrecked, yak_shaving and yonastoundingcastle;
# every one of them was a bracket line, and nothing else moved.
# Re-blessed again 2026-08-24 for the object *seen* model (see the block above
# the renegade_brainwave row).  humbug needs no route change -- exactly ONE
# line moved across the whole 832-command replay, `X teeth` at command 723,
# and the new answer is the RIGHT one: the live run400 transcript
# (~/adrift-battle/runner/wine/pfx/drive_c/adrift/Adrift_29_humbug.txt) answers
# "Nothing Special." there, where Scarier used to print Jasper's gold-teeth
# description.  Grandad's teeth are a part-of-character static of an NPC the
# player has never had described, so they are not seen and the examine falls
# through to the default.  That single line is the second live confirmation
# the seen model rests on.
# Re-blessed again 2026-08-24, with twenty-eight other rows, for the version
# gate on "(Getting off X first)" / "(Standing up first)" (sclibrar.cpp
# lib_go).  This row is where it was measured: command 254, a bare "W" off the
# stool, gets no such line from run400 at all.  It is NOT a parser or a
# position bug -- both lines are bracketed *references*, and from 3.9 on the
# Runner puts them behind Options -> Display & Media... -> Appearance ->
# "References in brackets", the same checkbox that gates the pronoun echo
# removed above.  That box starts unticked on every launch and is never
# restored from the registry, so the default Runner prints neither line:
#   run390 loc_431911 / loc_4319A0  test m_showbrackets.Checked by name
#   run400 loc_450339 / loc_4503BF  test MemVar_4942BA, the byte saved as
#                                   "showbrackets" at 4679A1 -- and the same
#                                   byte the "References in brackets" echo is
#                                   already known to hang on (48A095)
# 3.7 and 3.8 have no such menu and print both lines unconditionally (run370
# loc_42303C / loc_423078, run380 loc_428244 / loc_428280), so the gate is
# `< TAF_VERSION_390` and no pre-3.9 row moved.  43 lines went, all deletions,
# across 29 rows; every one of them was a bracket line and nothing else.
# Re-blessed a third time 2026-08-24, with thirty other rows, for the ADRIFT
# 4.0 OUTPUT FILTER.  This row is the lead: command 217 `Put sweet on plinth`
# answers "Okay.  Okay.  I put the sweet on the plinth." in run400
# (Adrift_30_humbug.txt:841) where Scarier said "Okay." once.  Neither "Okay."
# is authored -- task 80's CompleteText is a bare "I put the sweet on the
# plinth." (SCR_DUMP_TASKS=1) -- both come from the game's own ALR
# [I put ] -> [Okay.  I put ], whose replacement contains its own original.
#
# Three probes, packed with taftool.py and replayed in Wine, measured the rule
# (see harness/make_400_alr*probe.py and make_400_varfreezeprobe.py for the
# cells and the transcripts):
#
#   * A walk of the ALR list is a full length-descending pass repeated until a
#     pass changes nothing, with a self-containing ALR retired for the rest of
#     the walk it fired in.  3.9 is exactly ONE plain pass (run390 answers
#     "qAAA."/"PPPP." where run400 answers "qqAAA."/"QQ.").
#   * A 4.0 turn walks its WHOLE accumulated buffer once at the end of every
#     task that completes -- including tasks an action executes, at any depth,
#     and tasks an event's TaskAffected runs -- and once more at the flush.
#     Hence humbug's two "Okay."s, and hence sophie's [north] -> [north (to
#     the farmhouse)] repeating once per completing task.
#   * That pass interpolates variables too, so it FREEZES them at the
#     completing task.  A task that prints "%v%", runs another task, then
#     changes v shows the OLD value (run400: "A n=5 qqqball.").
#
# The last one bites 3monkeys, whose "chimp" task prints
# [CHIMPSIGNAL=%signal_to_chimp%] -- an ALR original built out of the variable
# -- before the action that increments it.  run400 prints the raw token; that
# was measured on the game itself, not argued (Adrift_16.txt, the first 36
# commands of the solution, every one echoed).  the_town_of_azra's win marker
# moved with it, 27 turns -> 26: its end-of-game summary freezes the turn
# counter at the completing task, before that turn's event bumps it.
# 2026-08-25 -- the phase-A transcript (Adrift_30_humbug.txt, solution lines
# 1..165) was swept with compare_wine_transcript.py: 165 of 165 commands
# echoed, ten differing turns, and ALL TEN are RNG.  Nine are Schrodinger the
# cat and one is the slate's roman numerals.  The cat looks exactly like a
# walk-tick divergence and is not one: SCR_SEED=1/2/3/12345/999 all announce
# the first step on the same turn, but each picks a different DESTINATION, so
# which rooms the cat is in -- and therefore which turns mention it at all --
# is a die roll.  Nothing to chase; do not re-sweep this transcript.
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
# 3.80, despite sitting in the 3.90 block.  Re-blessed 2026-08-24 with the
# other eighteen pre-3.9 rows; see the comment block above the akron row.
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
# Measured live in run400 under Wine (2026-08-24), the whole walkthrough with a
# 96-command peddler hunt spliced in: the Runner reaches "Congratulations!  You
# got Katryn's ending." / "Your Secret Letter is: E", with Laurie following the
# player 24 times.  That measurement is what fixed the walk ticker's expiry
# sentinel.  Laurie is NPC 2 and has fourteen walks; the one that carries her
# after the player is WALK 3 (loop, StartTask 415, StoppingTask 416, MeetChar
# 29).  Six of the walks above it -- 5, 6, 7, 9, 10 and 12 -- are non-looping
# with no StoppingTask at all, so once their start tasks have fired nothing in
# the game ever switches them off again, and run400's precedence scan
# (Proc_19_1_468DA0 @4686E7-4687C3) lets a higher-numbered walk shut a lower one
# down while its counter is above zero.  Everything therefore turns on what a
# finished 4.0 walk's counter holds.  The stamp at 46860B is `push &HFF 'Byte`,
# which is a SIGNED byte -- P32Dasm renders it "LitI2_Byte: 255 (True)", and the
# same opcode with the same operand is the Step of the descending stop scan at
# 468805, where it can only be -1.  Read as 255 it is a 256-turn countdown: the
# six walks keep restarting, sit above zero almost permanently, and pin WALK 3
# shut from the R.O.S.A. complex onwards, so Laurie is left behind at the ladder
# and "Aren't you forgetting Laurie?" ends the game two rooms short.  Read as -1
# it is inert -- never decremented, never restarted, never run, never in the way
# -- which is also why 4.0 could drop the pre-4.0 "looping walks only" test from
# its restart branch at 468675.  The engine now stamps -1 and matches the
# Runner: 24 follows and the win.
# The seven remaining differences against the pre-fix golden are all Laurie's
# in-room line, and the Runner confirms every one of them: it prints "Laurie is
# waiting for you under the lamppost." exactly ONCE, where the old golden had it
# eight times over, and otherwise the plain or task-selected description
# ("standing here", "by the stovetop cooking something", "sitting at the table",
# "lying on the floor").  Still open and NOT walk-related: on the second
# Detainment visit the Runner prints "Laurie is standing here." where the engine
# picks the alternate description "Laurie is in your arms.", and the game's
# timed events run a turn out of step with the Runner's in a good many places --
# the same offset class already noted on orient_express.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
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
#     "A sentry charges in" is.  The
#     palace therefore cannot be cleared, only outrun: the four timers are
#     staggered, so a room is empty for a turn or two at a time and the route
#     has to spend that turn moving.
#   * Leaving is blocked while anything is in the room ("Irvine has to deal
#     with his enemies before he can leave!"), so every doorway costs a full
#     sweep of whoever has cycled back in -- one correct swing each, since a
#     KO sticks until that mook's own respawn timer comes round again.  The
#     elite is the exception: it is not one of the four, it does not block a
#     doorway, and the route simply walks past it into the throne hall while
#     it is still standing there burning Irvine with its heat gun.
#   * `claw` (TASK 1217) is an area attack that hits every enemy present at
#     once, and it is the ONLY thing that touches the elite -- TASK 1292
#     `#elite_clawed_(POW!)` carries four "NPC not in room" restrictions, one
#     per mook, which is the "elite must be alone" rule from the PDF.  It is
#     gated on `claw_count >= 3` and resets the counter to 0, and every
#     attack (hit or miss) bumps the counter by one, so it recharges over
#     three swings.  The route never spends it in the palace: nothing there
#     needs an area attack and the elite need not be fought at all.
#   * Health is a damage counter, not a pool: VAR 41 [Irvine_Health] starts a
#     fight at 0 and each `#<mook>_attack` adds 1 for every enemy standing in
#     the room; VAR 63 [HP] is only the mirror (TASK 1342 ###IRVINE_HEALTH###
#     dispatches TASK 1349-1361 #IrH0..#IrH12, each setting HP = 12 - damage).
#     At damage 12 TASK 1343 #Irvine_LifeCheck fires, and inside the palace
#     (rooms 42-53) TASK 1347 #imprisoned! throws Irvine in room 62.  TASK
#     1348 #heal_over_time takes one damage back off and EVENT 45 [Heal Over
#     Time] runs it every 3-6 turns.  `breathe` (one damage off a turn,
#     refused unless Irvine is alone) is the only repair the player can aim,
#     and it does work -- three turns take a badly hurt Irvine back to a full
#     12 -- but the respawn lands on the fourth quiet turn wherever the player
#     is, so topping up just hands the wave back at the wrong moment; going
#     straight through turned out to be cheaper than healing first, and the
#     route never comes near the damage cap.
# The two health-restoring objects the tasks talk about are unreachable: the
# health pill (obj310) has no Where node at all and no action anywhere moves
# it, so OBJLOC reports pos=-1 room=-1 for the whole game.
#
# The chapter 6 fan-servant scene (`teach fan karate`, `give jacket to fan`,
# `ask for help`) is optional by the PDF's own admission; it is kept because
# the epilogue calls back to it ("Where's your coat?" / "Gave it away.").
#
# Chapter 5 re-derived 2026-08-24 (185 commands down to 178, everything from
# `sweep guard` on) for the walk-precedence fix.  The old route swung twice at
# the patrol at four separate doorways because a KO'd patrol came straight
# back the same turn: NPC 16's WALK 1 is a one-stop follow-the-player walk
# started by TASK 1276 #patrol_clawed, and Scarier used to run it, which is
# what "Patrol charges after Irvine." was.  The real Runner never runs it.
# NPC 16 also carries a WALK 2 with StartTask 0, StoppingTask 0 and no stops
# at all, and run400's precedence scan (Proc_19_1_468DA0, walk loop
# 4686E7-4687C3) lets a StartTask-0 walk suppress every lower-numbered walk
# with no test of its counter and none of its stop count -- so WALK 2 pins
# both WALK 0 and WALK 1 shut for the whole game.  That is the same shape as
# The Fun House's bouncer, whose 18-command replay was measured live in
# run400 under Wine and matches the fixed engine exactly; this row leans on
# that measurement instead of one of its own, because chapter 5 is driven by
# respawn timers that re-roll on any change of turn count, so a
# command-for-command Wine replay of a 178-turn route proves nothing.  The
# attribution was pinned down by construction as well: suppress the empty
# walk's precedence and the pre-fix route reproduces the pre-fix golden byte
# for byte, change nothing else and it does not.
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
# 2026-08-25: one line moves with the "The"-prefix fix (the Runner's tense
# has no "the" branch -- see the xfiles block below).
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
# its route is derived (the corpus is now complete).  Win markers
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
#
# 2026-08-24 -- ALL NINETEEN PRE-3.9 ROWS RE-BLESSED.  The nineteen rows in
# this suite that come from a 3.70 or 3.80 .taf (the seventeen listed above,
# plus marooned and wrecked further up and Crime_Adventure in the 3.90 block)
# all changed together, because five wordings that scarier had only ever
# measured on 3.9 and 4.0 turn out to be later inventions.  Three walkthroughs
# were replayed command-for-command in the real Runner under Wine to pin them:
# arlo.taf (3.70) in run370, and akron.taf and mikes.taf (3.80) in run380.
# akron now matches run380 exactly, 0 differing commands out of 44.
#
#   1. No room-name heading at all.  "showshortroom" occurs eight times in the
#      run390 P-code and twice in run400's, and not once in run370's or
#      run380's -- their Options -> Appearance submenu offers colours and a
#      font size only.  Verbose mode prints the description with no heading
#      above it; brief mode prints the room name with a trailing period as the
#      whole of the description.  This accounts for nearly every deleted line
#      in this re-bless.
#   2. "remove" prints the object's prefix raw: "You remove a grubby
#      sweatshirt.", not "the".  run370 @42980D and run380 @42FF38 concatenate
#      it; run390 routes the same listing through General.Sub_3_45.
#   3. tense() is a much smaller normalizer -- run370 @420F28 and run380
#      @425FA8 are byte-identical, three literal tests and no default.  Exact
#      "a" -> "the", leading "a " or "an " -> "the ", everything else passes
#      through, so a bare "an" survives: run370 answers `take implement of
#      destruction` with "You pick up an implement of destruction."  (A prefix
#      the author used as part of the noun phrase passes through too, which is
#      why haunt keeps "Inside kitchen cupboard is vial of liquid." -- its
#      Prefix really is "kitchen" and its Short "cupboard".)
#   4. The from-container multi-take prints raw as well.  3.9 normalizes it
#      (run390 on ALEXIS: `get all from table` -> "the diary, the brass
#      lantern, ..."); run380 on mikes gives "You take a socks, a shirt, a
#      underwear and a pair of pants from the dresser."
#   5. There is no alternate container or surface listing.  scarier picks
#      "<obj> is inside <cont>." for one or two contents and "Inside <cont> is
#      <list>." for three or more, a rule derived from run400.  Pre-3.9 there
#      is nothing to pick: run370 and run380 carry only the "  Inside " and
#      "  On " literals (run380 @43D0B1), and " is inside ", " are inside ",
#      " is on " and " are on " are all absent from both binaries and first
#      appear in run390.  run380 on mikes answers `open toilet`, one content,
#      with "Inside the toilet is a poop."
#
# An empty Prefix is deliberately NOT on that list, and the reason is worth
# recording because the opposite was believed for a while.  tense() has no
# empty-prefix branch, which looks like it should leave pre-3.9 with no
# article -- but the empty prefix never reaches tense(): the loader rewrites
# it.  run380 @4481B2 and run370 @43F5DA read the Prefix line and, if it is
# empty, substitute a literal "a".  So empty simply *is* "a" before 3.9, and
# scarier's long-standing defaults ("the " normalized, "a " raw) were right
# all along.  mikes.taf is the disproof: its dresser, toilet, poop and vans
# all carry an empty Prefix -- deobfuscated straight out of the .taf, not
# inferred -- and run380 still says "You pick up the pair of vans."
#
# Still open against the Runner, and not deviations these goldens claim to
# settle: arlo's NPC walks.  run370 prints departure lines scarier omits
# ("Rude Customer walks off.", "Alice walks off to ..."), which desynchronises
# presence state by command 33, and `get out of bus` at the church ends with
# the task's "You are no longer in the bus." and no exits list where scarier
# prints the exits and drops the task line.  Six of arlo's 84 commands differ
# for those two reasons alone.  See notes/WINE-TRANSCRIPTS-TODO.md.
akron_solution.txt|akron.taf|you brave adventurer, saved yourself
cave_solution.txt|cave.taf|You scored 1000 out of the maximum 1000!
haunt_solution.txt|haunt.taf|You scored 84 out of the maximum 84!
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
twilight_solution.txt|twilight.taf|Your score is 500 out of a maximum of 500
haunted_house_solution.txt|haunted.taf|You scored 1000 out of the maximum 1000!
# 3.80.  Re-blessed 2026-08-24: "Mrs Walters totters into the room." moves
# about six lines later in the transcript -- she is announced on a later turn,
# not lost.  Not measured under Wine (3.80 needs the Save-Transcript-at-end
# .rtf flow); justified from run380's P-code, which decompiles far more
# readably than run390's and says the same thing as run400:
#   Public Sub characters() '441928
#     loc_4412E4  counter countdown -- a PLAIN decrement, with NO 0xFF expiry
#                 stamp (that is a 4.0-only addition)
#     loc_441389  restart gate: ((counter < 0) Or (counter = 0 And Loop = 1))
#                 And ok   -- i.e. a non-looping 3.8 walk never restarts
#     loc_4413FB  For other = self+1 To NumWalks-1
#                   If walk(other).StartTask = 0 Then ok = 0
#                   Else If done(walk(other).StartTask)
#                           And walk(other).counter > 0 Then ok = 0
#                 -- note there is NO StoppingTask test at all, because 3.7
#                 and 3.8 have no such field.  npc_walk_property() returns 0
#                 for it there, so Scarier's single npc_walk_preempts()
#                 already degrades to exactly this.
# npc_tick_npc() gates the 0xFF stamp on is_400 and the restart on
# (is_400 || npc_walk_is_loop(...)), so the 3.8 path matches line for line.
great_escape_solution.txt|great.taf|cry of joy, you have made it, you have escaped!!
tom_ceader_solution.txt|secret.taf|you did good work escaping from the town
# The 3.8 half of the walk-announcement rewrite was measured on this game --
# run380 under Wine, Adven_9_timmy_reid.rtf, 2026-08-24: Hovey's departure really is
# "shuffles off outside." with no "to".  See the arlo block.
timmy_reid_solution.txt|tra.taf|Thanks for getting us back home!
duck_mccloud_solution.txt|duck.taf|You jump from the plane just in time and you survive the huge
fistandantalus_solution.txt|first.taf|Congradulations you have won the game
james_bond_solution.txt|jb2000.taf|YOU COMPLEATED THE MISSION! YOU LANDED WELL
microwave_man_solution.txt|microwaveman.taf|You scored 100 out of the maximum 100!
# DIAGNOSED 2026-08-24, deliberately not ported -- cmd 27 `take truck keys`.
# run380 (Adven_8_mikes.rtf line 207) answers "Which keys.  The mustang keys or the
# truck keys?" and does NOT take them; scarier binds the truck keys silently,
# and every difference from cmd 53 on is downstream of that.
#
# The mechanism is the Runner's `co(obnum)` (run380 @42DE60, run370 @4261B4,
# run390 `co(obnum, mode)` @43B6BC -- one routine, same shape in all three).
# Where our %object% matcher is positional, the Runner scans the WHOLE typed
# command for each object's Short name and, failing that, its Alias (`c()`
# @429048: case-insensitive InStr whose hit must start at the string start or
# after a space and end at the string end, a space or a comma).  It then takes
# the term that matched, counts every object PRESENT whose Short or Alias is
# exactly that term, and if more than one is present flags the command
# ambiguous -- the flag (MemVar_44F124) is read at the end of the turn
# @4431B0 and REPLACES the whole turn's output with "Which <term>.  <list>?".
# One escape hatch: if the player also typed the last word of the object's own
# Prefix ("take *silver* key"), @42DD4C stamps the resolved marker &HFE
# instead, and that marker outranks any ambiguity raised by any other object
# in the same scan (@42DDC1 only writes an object number when the marker is
# not already set).  mikes' two key objects have an empty Prefix, so nothing
# rescues them: Short "mustang keys"/Alias "keys" and Short "truck keys"/Alias
# "keys", both present at cmd 27 because the mustang keys were taken at cmd 8.
# `take mustang keys` at cmd 8 is NOT ambiguous only because the truck keys
# were not yet in scope.
#
# Measured across the corpus with the SCR_TRACE_CO diagnostic added to
# sclibrar.cpp for this (it reproduces co() alongside our own matcher and
# prints CO-AMBIG when the Runner would have asked and we did not):
#   `for row; do SCR_TRACE_CO=1 scare games/$taf < goldens/$sol; done`
# 31 commands in 14 games trip the Runner's test.  19 of them are commands our
# own disambiguator already calls ambiguous, so only the wording differs.  Of
# the remaining 12, ELEVEN are in 4.00 games -- and 4.00 is exactly the
# generation where the model is not established, because `[1]$Alias` becomes
# `V$Alias` there (3.7/3.8/3.9 objects carry exactly one alias, which is why
# co() can read a single field 8; a 4.00 object carries a list).  Taking all
# of a 4.00 object's aliases as co() terms would make `open second valve`
# ambiguous in asteroid_after (six valves, each aliased "valve", Prefix "the"),
# i.e. would make that game unplayable -- which is good evidence that run400
# does something else.  run400 keeps its messages in a table, so neither
# run400.bas nor run400.p32dasm.txt resolves the "Which " literal (it is in
# the .exe at file offset 0x17a2c), and this cannot be read off the decompile.
#
# So at 3.7/3.8/3.9, where the rule IS established, mikes cmd 27 is the corpus'
# ONLY divergence -- one row, whose walkthrough would then need re-deriving
# (drop the mustang keys first, presumably).  Porting on that alone would mean
# writing a rule for 4.00 games we have not measured.  NEXT STEP: one live
# command settles it -- run asteroid_after in run400 under Wine and type
# `open second valve`.  A prompt means the 4.00 rule is the same and the port
# is one code path; a normal answer means 4.00 narrows by the longest match
# and the port has to be version-split.
life_of_mike_solution.txt|mikes.taf|Ypu ask her out
super_liam_solution.txt|superliam.taf|congradulation you have defeated x1
# The 3.7 confirmation of the empty-M1 room-alt start rule (see the
# lair-of-the-cybercow rows): measured live in run370 under Wine, cmd 34 is now
# byte-exact against Adven_6_arlo.rtf, taking arlo from 7 differing commands of 85
# down to 6.  Note this corrects an earlier diagnosis recorded in
# notes/WINE-TRANSCRIPTS-TODO.md, which blamed cmd 34 on NPC-walk presence
# desync; it is a room-alt selection bug and is unrelated to the walk departure
# announcements, which the block below then ported.
#
# 2026-08-24 -- NPC walk announcements rewritten against the Runner's own
# wherefrom() (run370 @422F8C, run380 @42800C, run390 @430200, run400
# Proc_19_20 @45234C -- one routine, unchanged across all four).  Twenty
# goldens moved; the four generations were each measured live under Wine to
# pin the parts that differ.  What changed:
#
#   * DIRECTION IS NAMED FROM THE OTHER ROOM, REVERSED.  SCARE scanned the
#     *player's* room forward for the NPC's room and took the first match.
#     The Runner scans the *other* room's exits for the player's room, names
#     that exit's opposite, and lets the LAST match win (no early break).  On
#     a symmetric map the two agree; on a one-way map they do not, which is
#     why provenance's butler gained "to the west" at three sites where the
#     old forward scan found nothing.
#   * EIGHTPOINTCOMPASS IS NEVER CONSULTED.  Pre-4.0 scans exits 0..7, 4.0
#     scans 0..11, whatever the flag says -- so a diagonal move is nameless
#     before 4.0.  That is alexis' 28 lost "from the south-east"/"north-west"
#     clauses and stardust's, and melbourne_beach's "David strolls in."
#   * THE DEPARTURE GATE IS VERSION-SPLIT.  wherefrom() answers two
#     non-directions, "not moved" and "nowhere", and each Runner suppresses a
#     different pair: 3.7 suppresses only "nowhere" (so "Alice walks off to
#     not moved." really is what run370 prints, twice, in arlo); 3.8 and 3.9
#     suppress both; 4.0 suppresses only "not moved" and prints a bare
#     "X walks off." on "nowhere".  baroo's "Wizard strides off toward the
#     hotel and enters it." and cursed's "Lord Vonisor exits through the door
#     and joins you in the corridor." are 4.0 bare lines -- author-written
#     self-contained sentences that had been getting a bogus " to the east"
#     bolted on.
#   * "outside" LOSES THE "to".  Every Runner special-cases it: "walks off
#     outside.", not "walks off to outside." (run380 @0004160D, run390
#     loc_45A840, run400 loc_46891E).  timmy_reid's Hovey is the corpus case.
#   * A FOLLOW-PLAYER STOP IS ANNOUNCED BY NO RUNNER, even though the walker
#     is about to warp to the player.  4.0 gates the departure branch on the
#     resolved destination being a real room (run400 @4688B0, var_BE > 0, and
#     a follow stop resolves to the not-a-room zero); 3.7/3.8/3.9 do reach the
#     branch but hand wherefrom() that same zero, whose exit-less dummy room
#     answers "nowhere", which all three suppress.
#   * A HIDDEN STOP gets a directionless line in 3.7 and 4.0 ONLY (run370
#     loc_4397A3, run400 loc_468CF9, and no resource is played).  run380
#     loc_4418DD and run390 have nothing in that branch but the "stamp the NPC
#     nowhere" assignment, so a 3.8/3.9 walker vanishes in silence.  arlo's
#     "Rude Customer walks off." is the 3.7 case.
#   * BOTH LINES FIRE ONLY ON THE EXACT TICK, the turn the walk counter lands
#     on this stop's suffix sum -- run400 loc_468841 is
#     `If (counter = sum) And (sum > 0)` branching false straight to
#     loc_468D51, the loop's Next, so a multi-turn stay is announced once, not
#     once per turn.  This is what dropped provenance's three bare "The butler
#     exits." lines: they fired on non-exact ticks after a task had displaced
#     the butler.
#
# Measured live under Wine, one game per generation:
#   3.7  arlo / run370 / Adven_6_arlo.rtf -- all three departure lines now match;
#        arlo is down to 3 differing commands of 85 (the two `get out of bus`
#        rows and the transcript-end artefact, both still open).
#   3.8  timmy_reid / run380 / Adven_9_timmy_reid.rtf -- "Hovey shuffles off outside.",
#        with no "to".  The old golden's "to outside" was wrong.
#   3.9  melbourne_beach / run390 / Adrift_37_melbourne_beach.txt -- all four changed sites:
#        "Kitty departs to inside.", "Kitty comes in from the west."
#        (direction gained), "Kitty enters." (direction dropped) and
#        "David strolls in." (diagonal dropped, 8-exit scan).  Only the NPC
#        verb text differs, never the direction -- those are the walk's random
#        alternate texts.
#   4.0  orient_express / run400 / Adrift_36_orient_express.txt -- every walk line matches,
#        including "Ivanna Stiffdrink walks off to the west.", "Ivanna
#        Stiffdrink walks towards you from inside." and "Oddly Istink wobbles
#        in from the east."
#
# Still open, and logged in notes/WINE-TRANSCRIPTS-TODO.md rather than fixed
# here: 3.8/3.9/4.0 add one more arrival test we cannot make -- the walker's
# pre-move location must not be the Runner's not-a-room zero (run380 @4416F4,
# run390 loc_45A99B, run400 @468A5D).  The Runner spells "not a room" two
# ways, 0 for an NPC the game never placed and 0xFF for one a walk has just
# hidden; Scarier collapses both into one marker, so it cannot tell them
# apart.  orient_express is the live proof this matters: Scarier prints
# "Gimme Atip enters." where run400 prints nothing.
#
# 2026-08-25 -- THE ANNOUNCEMENT IS JOINED INTO THE TURN'S PARAGRAPH, not
# given a line of its own, and 61 goldens moved for it.  Every Runner appends
# the walk lines to the same buffer the rest of the turn is being built in,
# with the two-space section separator:
#
#   run370 loc_4395AA / run380 loc_441740
#       If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "
#   run390 loc_45A99E / run400 loc_468A67 (arrival) and loc_4688D3 (departure)
#       Call pspace()    ' the same test plus "already ends in two spaces"
#                        ' and "already ends in <br>"
#
# so the pre-3.9 form really does make four spaces where the text already
# ended in two, and 3.9/4.0 does not -- hence pf_buffer_join() (pspace) and
# pf_buffer_join_always() (the older inline form) in scprintf.cpp, picked by
# version in npc_announce().  The 3.7/4.0 HIDDEN-stop line (run370 loc_4397A3,
# run400 loc_468CF9) appends a bare "  " with no pspace call at all.
#
# This is not cosmetic.  The joined sentence lands in the buffer the ALR pass
# later walks, so an author can write an ALR whose Original spans the join:
#
#   sa.taf / sophie.taf (4.00) carry 65 of them, e.g.
#       'quiet.  Grumble complaining of beer deprivation staggers in from the
#        west.'  ->  'quiet.'
#   and 12 fire in this walkthrough, which is why Grumble's arrival vanishes
#   at named spots.  Buffered on a line of its own, as Scarier used to, no
#   such ALR can ever match, and every one of those 12 lines survived.
#   Bisected live in sa.taf under Wine (deleting all 488 ALRs made the silent
#   turns announce), then reproduced from scratch by
#   harness/make_400_walkalrprobe.py under run400 (Adrift_47_p4walkalr.txt): its two
#   cross-the-join ALRs both fire and the single-space twin of one of them
#   does not, so the separator is two spaces and not "some whitespace".
#
#   circus.taf (3.90) shows an author building ON the join rather than
#   deleting through it -- for its NPC named "Joe" it carries the pair
#       '  Joe'  ->  '  The vendor'        (the joined, sentence-initial case)
#       'Joe'    ->  'the vendor'          (everywhere else)
#   and the first of those two now fires, which is why circus's three vendor
#   lines gained their capital.
#
# CAPITALISING THE NAME IS A 4.0-ONLY STEP, and the join is what made it
# visible.  3.7/3.8/3.9 concatenate the Name verbatim (run370 loc_43961A,
# run380 loc_4417B0, run390 loc_45AA0F); 4.0 puts it through the Runner's
# one-line capitaliser first at both npc_announce() sites (run400 loc_468A79
# and loc_4688E0 call Proc_21_3_446BB4 = UCase(Left(s,1)) & Right(s,Len(s)-1),
# General.bas:75) and NOT at the hidden-stop site (loc_468CF9).  SCARE used to
# capitalise unconditionally.  baroo.taf (4.00) is the corpus case both ways:
# its walkers are named "wizard" and "warlock" with Prefix "the", and 4.0
# really does print "Wizard strides off to the east."  A pre-4.0 game in the
# same shape would not.  harness/make_400_walkcapprobe.py is the live
# confirmation, built and not yet run (the desktop was locked).
#
# Cross-checked against every committed Runner transcript that has a walker,
# one per generation:
#   3.7  arlo / Adven_6_arlo.rtf -- "The man shrugs and goes home.  Rude
#        Customer walks off." and "...You can move south, west and out.  Alice
#        walks off to not moved."
#   3.8  timmy_reid / Adven_9_timmy_reid.rtf -- "Hovey is here.  Hovey shuffles
#        off to the north.  The faint odor of snappers wafts towards you from
#        the east." -- three sections, one paragraph.
#   3.9  melbourne_beach / Adrift_37_melbourne_beach.txt -- all five sites,
#        e.g. "...four pockets you can put small items in.  Kitty comes in from
#        the west." and "Kitchen.  David strolls in."
#   3.9  stardust / Adrift_38_stardust.txt -- "You can move north and south.
#        Squirrel scampers up from the north." and the rest of its 129 lines.
#   4.0  orient_express / Adrift_36_orient_express.txt -- "Dining Car.  Ivanna
#        Stiffdrink walks towards you from the south.  The train whistle blows
#        loudly, causing a slight ringing in your ears."
#
# Of the 61 goldens this moved, 57 differ from their predecessors in
# whitespace alone; the other four are baroo (the capital), circus (the '  Joe'
# ALR) and the two sophie rows (the 12 deleted arrivals).
# arlo.taf (3.70) -- one measured, deliberate deviation, diagnosed 2026-08-24
# against run370.exe under Wine (Adven_10_arlo.rtf) and the run370 p-code.
#
#     > get out of bus                                    (at the church)
#     run370:  You're on foot.  <room 0 description> ... There is a mailbox
#              here.  You are no longer in the bus.        [one paragraph]
#     scarier: You're on foot. / <room 0 description> ... There is a mailbox
#              here. / <blank> / You can move north and east.
#
# The Runner runs the task matcher TWICE for this command.  takes() is entered
# for anything containing get/take/pick without "from" (@00035D8C), and on the
# first object whose name or alias is in the command -- object 29 "microbus",
# alias "bus" -- it calls the matcher with mode 1 and the player's ORIGINAL
# command (@00036CAD).  That completes task 72, whose ShowRoomDesc runs
# viewroom(room 0); viewroom prints everything accumulated immediately and
# without a newline and leaves ONLY the exits sentence in the buffer
# (@0003315C).  takes() then returns Empty -- it has no store to its result
# slot at all, the "takes = MemVar_4460E4" in run370.bas is VB Decompiler's
# rendering of ExitProc -- so generaltasks falls through to its own matcher
# call, mode 0 (@0003B972).  Task 72's Where gate now fails (its Movements
# moved the player to room 0) and task 107 matches instead, and mode 0
# CLOBBERS the buffer with its CompleteText (@00041C21).  The exits sentence
# is destroyed before it is ever flushed.
#
# Not ported: scarier runs the matcher once, and the second pass would need
# clobber-the-buffer semantics its filter has no equivalent for.  The
# preconditions -- a take-family command naming an object, matching a
# REPEATABLE task whose own effects make a DIFFERENT task match -- are hit by
# no other game in this corpus.  Full write-up in
# notes/WINE-TRANSCRIPTS-TODO.md, "the run370 double matcher pass".
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
# baroo names its walkers "wizard" and "warlock" (Prefix "the") in lower case,
# and this is the corpus case for the 4.0-only Name capitaliser: run400 prints
# "Wizard strides off to the east." where a 3.9 game in the same shape would
# print "wizard".  See the 2026-08-25 block above.
baroo_solution.txt|baroo.taf|You scored 16 out of the maximum 16!
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
law_solution.txt|I am the Law.taf|out for Enterprise Research.|SCR_SKIP_WAITKEY=1
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
# Re-blessed 2026-08-25, one line (859, `open satchel`): the same "Open what?"
# -> "You can't open that." fix recorded on the xfiles_solution.txt row above.
# Re-blessed again 2026-08-25, two lines, by the 4.0 seen-but-absent resolver
# (`x dust` -> "You can't see the dust from here!", `open satchel` -> "You
# can't see the satchel."): TheCellar.taf is a 4.00 file, and both nouns name
# an object the player has seen but is no longer in the room with, so 4.0's
# second binding pass finds it where 3.9's never looks.  Measured on p4EXAM.taf
# under run400 and on p39EXAM.taf under run390; see notes/WINE-TRANSCRIPTS-TODO.md,
# "FIXED 2026-08-25 -- the 4.0 seen-but-absent resolver".
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
# Re-blessed 2026-08-25, one line (470, `read eye`): the pre-4.0 read-refusal
# wording recorded on the cybercow_win_solution.txt row above.  As there, the
# visible line is the game's own ALR -- panic.taf maps [Nothing special.] ->
# [I do not discern the object you want to examine.]  That ALR table is the
# corpus's most complete offline oracle for Runner wording: 200-odd Originals,
# enumerated by the author, and it is what independently confirmed that no
# Runner says "Open what?".
panic_solution.txt|panic.taf|Your rating is Messiah.|SCR_SKIP_WAITKEY=1
# --- 2026-08-11: unwired 3.9 games, smallest first -------------------------
# I... (Christopher Cole) -- one-room, no scoring system.  The whole game is a
# four-link event chain: `feel pulse` starts Memory 1, and each memory event
# starts the next; Memory 4 is the task with the EndGame action.  The leading
# flavour commands are the game's other seven tasks (they do not touch the
# chain); the 21 `z`s are the measured 4+5+7+5 turn timers under the fixed seed.
i_solution.txt|i.taf|I am dead.
# Dreamland (Daniel Bergman) -- one room, four tasks, one scoring action.
# `fill waterskin with water` then `pour ... into basin` is the whole game;
# everything else is scenery, and EVENT 0 kills you on turn 35.  The leading
# blank line answers the intro's "Click any button".
dreamland_solution.txt|Dreams.taf|You have saved the Dreamworld
# Forest On The Norm (Tobias Schmitt, RON 2002) -- a 16-room corridor whose
# every door is gated on one task in the room before it, and no score at all
# (`My score is 0 out of a maximum of 0`).  TASK 15 `show end` prints the
# closing credits but has no EndGame action, so the marker is that text.
forest_on_the_norm_solution.txt|forest.taf|Thank you for playing my Aliengame
# The Adventures of Bob Bobsly -- 155/155, every one of the ten scoring tasks.
# `take gum` in The Bar is the one non-obvious step: `chew gum` is a
# where=anywhere task that just answers "You don't have the right equipment"
# until you are holding the wad, and the coin it yields is the only money.
bob_bobsly_solution.txt|BobBobsly.taf|You scored 155 out of the maximum 155!
# Druggy Lane -- a Dope Wars clone: one room, 23 variables, 30 days.  Prices
# are re-randomised by `next day`, so the route is a seed-specific trading
# plan derived offline from a measured price table; the filler turns are
# `look` on purpose (`wait` runs Globals.WaitTurns turns and desyncs the RNG
# stream, an unparsed command consumes a different amount again).  Ends
# debt-free with $1,955,720,463 -- just under the 32-bit ceiling the real VB6
# Runner would overflow at.  See notes/Druggy_Lane_walkthrough.md.
druggy_lane_solution.txt|druggy_lane.taf|You have managed to deal your way to freedom!
# Escape from Insanity -- 1000/1000, one padded cell, a six-step tool chain.
# `use rock on button` is typed twice on purpose: TASK 11 claims it first and
# pops the button off the wall, and only once TASK 11 is done (rep=0) does
# TASK 12 get the same pattern and cut the knife out of it.
escape_from_insanity_solution.txt|Insane.taf|Congratulations psychopath, you're now a pyro.
# Lost Souls: the trunk in the Attic is guarded by two same-pattern tasks --
# TASK 16 (unrestricted, repeatable, "it's locked") shadows TASK 17 (needs the
# key) for every `open ... trunk` phrasing.  Only TASK 17's extra alternatives
# `unlock trunk` / `use key on trunk` reach it, and the scrap of paper it
# yields is what unlocks `d` in the Kitchen.  See notes/Lost_Souls_walkthrough.md.
lost_souls_solution.txt|lostsouls.taf|You don't want to go down there.
# Chicago: 75, the sum of every ACT type=4 (the status line's "maximum" is 0,
# the author never set one).  `confront daisy` is the only winning end; the
# other two `confront` tasks are instant losses.
chicago_solution.txt|chicago.taf|Daisy was found guilty of double homicide
# Everything Emanuelle: no score; `out` wins from turn one, so the marker locks
# the ENDING -- %opinion%==5, the last of the four written ALTs.  Reading the
# diary sets %opinion% to 5 (it does not add), so 6/7 are reachable and print
# the author's unwritten "ending6"/"ending7" placeholders.
# Re-blessed 2026-08-25, two lines, for the pre-4.0 `x <unknown noun>` answer
# measured on the veteran row far above -- this is the corpus's only sighting of
# the first-person form, "I see no such thing." -> "Nothing special."
everything_solution.txt|everything.taf|I'll smile as I curse her name and everything Emanuelle.|SCR_SKIP_WAITKEY=1
# Textident Evil: 100/100 (the game's own stated maximum).  TURN-CRITICAL --
# four monster events run on a fixed global cadence and the zombie's WALKs
# re-teleport it onto the player after every kill, so the `instructions` turn
# and the exact command count before the dog fight are load-bearing.
textident_evil_solution.txt|Textident_Evil.taf|Congratulations! You've successfully beaten Textident Evil.|SCR_SEED=4
# Impulso: a Spanish conversation piece with no map, no objects and no
# score -- 12 tasks, all unrestricted, chained by ACT type=1 moves.  You
# reconstruct three murders for a journalist and the only failure mode is
# not finding the phrasing; a verb aimed at the wrong beat falls through to
# the runner's untranslated "You can't do that here!".  Every command below
# is the ASCII-only alternative of an accented pattern, so the solution file
# stays 7-bit even though the .taf is CP1252.
impulso_solution.txt|impulso.taf|Solo una cosa. Me di cuenta hace un cuarto de hora
# Montahue Scott and the Mobius Belt: 3/3 in one room.  Two order traps in
# eighteen tasks -- T3/T9/T10 all require task 8 NOT done, so Chelsea and Bo
# must be asked about the communicator BEFORE Virgil fixes it, and the
# shuttle T8 summons starts a 15-turn countdown (EVENT 1 -> T15, -1 point
# and ACT type=6 v1=2) that the flavour block below sits safely inside.
# Re-blessed 2026-08-25, one line (151, `x hole`): pre-4.0, an object whose
# Description is empty leaves the Runner's message string empty and falls
# through to the same tail an unmatched noun does -- "Nothing special." (run370
# 435BF4, run380 43D545, run390 44C3DC).  4.0 fills it in one branch earlier
# with "You see nothing special about <obj>." (run400 471A08/471A1C), which is
# what scarier printed for every version.  Measured on p39EXAM.taf's `x stone`,
# in the room (Adrift_41_p39exam.txt) and held (Adrift_43_p39exam.txt) -- same
# answer both ways -- against the 4.00 twin's (Adrift_1_p4exam.txt).  This is
# the corpus's only pre-4.0 exposure.
ms_mobius_solution.txt|ms_mobius.taf|That little TV screen for the inside of your hat was a good investment.
# A Morning with a Headache: 115/115, the game's own maximum.  Four fatal
# deadlines on a fixed global clock -- the buzzing alarm evicts you at turn
# 15, the girlfriend catches the stripper at 30, arrives (and teleports you)
# at 35, and the wedding leaves without you at 55 -- so the route is timed
# throughout.  The +3 requires leaving the alarm ON one turn longer than a
# player would, and `wash me` locks out the moment Hanna is forgiven.
morning_headache_solution.txt|A_Morning_with_a_Headache.taf|This has turned out to be an altogether OK morning.
# Sleaze City: 100/100, every ACT type=4 in the file.  A ten-room slum with
# no NPCs, no events and no clock -- the only real constraint is a four-link
# chain (buy tickets -> cut the chain for the newspaper -> read it -> hand the
# winning ticket to the landlord) plus the cafe door reversing direction:
# EXIT room=6 N is gateTask=18 wantDone=0 and IN is wantDone=1, so `talk to
# gimpy` closes the front entrance and opens the kitchen window behind you.
sleaze_solution.txt|sleaze.taf|You scored 100 out of the maximum 100!
# Albridge Manor: 50/50.  A 27-room haunted house whose endgame is a burial --
# T26 `bury crucifix` carries six restrictions (crucifix + shovel held, and
# T22..T25 all done), so the doll, necklace, pipe and ball go into the Secret
# Room floor first and the crucifix last.  The first two script lines are the
# name and gender prompts, not commands; the name is echoed back in the ghost
# whispers, so it has to stay stable for the golden.
manor_solution.txt|manor.taf|You bury the crucifix with the other items.
# The Lost Mines: 100/100.  The whole game is one chain of favours -- ring ->
# pencil -> signed coupon -> beer -> Gus leaves the tunnel -- and each link is
# a restriction on the next, so the order is forced.  Two containers refuse to
# be picked up and have to be opened in place (`open box` / `open pillow`),
# and T40 never actually checks for the dynamite: matchbook + a thirsty Gus is
# all the engine wants.
lostmines_solution.txt|lostmines.taf|Congratulations, you have found the lost gold.
# The Dark Tower: no score at all (zero ACT type=4), so the finish line is
# T8 `turn on power` -- eight tools held at once for the panel, four more for
# the generator, one per suite across five floors.  A key card is used ONCE
# and opens its whole floor, and everything outside the lobby and the garage
# is pitch dark until you find the flashlight under the van.  The elevators,
# the shafts and the entire black-van ending are unreachable: they all hang
# off T8, which ends the game.  "To be continued................"
darktower_solution.txt|DarkTower.taf|restored power to the building.
# Report Espionage: 100/100, and all 23 tasks fire.  Seven report cards, seven
# owners, and the whole game is prising them loose -- including swinging a
# bribed Year 8 student at the fire alarm to empty the staffroom.  The library
# door is a reversible task whose three exits disagree about it (room 4 W and
# room 5 E want it done, room 5 N wants it NOT done), so it is always `open
# door` / w / `close door` / n; re-opening does not re-score.  Mrs Walsh is the
# one wandering NPC and T10 needs her present, so the Correspondence Set is
# handed over on the Deck at the exact turn she is standing there.
report_solution.txt|report.taf|You scored 100 out of the maximum 100!
# Far From Home: 50/50 -- fourteen +3 tasks plus +8 for the riddle.  Two
# <waitkey> pauses eat a script line, so the file starts with a bare `x`
# BEFORE the name (the intro pauses before the name prompt) and carries a
# second one right after `climb beanstalk`.  Three one-way chapters: the
# water in the hole teleports you to the castle, the beanstalk to the ocean,
# and the box to the Puzzlelord -- nothing can be fetched back afterwards.
# The pirate walks away after one turn, so `give pearl to pirate` has to be
# the first command on the lighthouse's 4th floor.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
farfromhome_solution.txt|FarFromHome.taf|You scored 50 out of the maximum 50!|SCR_SKIP_WAITKEY=1
# S Tar Dus T: no score anywhere in the file, so the goal is the richest of
# the four `sw` endings.  All four are gated on T31 (the magic words) and
# then picked in file order by what else you did: T33 wants the lake water
# drunk, T34 the bracelet worn, T35 the needle boxed and the tea drunk, and
# T36 is the LOSING one you get when none of them match (you step through the
# portal and wake from a coma).  This route takes T35, the only ending whose
# conditions cost you nothing but two mistakes, so every optional scene is
# still on the way.  The missing page falls in the LAKE, not the outhouse:
# `ACT type=0` "into object" indexes the container list directly, no -1.
# ---------------------------------------------------------------------------
# 2026-08-24 -- the not-a-room-zero ARRIVAL gate.  Canonical block for the
# twenty goldens this moved; the walk *announcement* rewrite it completes is
# written up above the alices_restaurant row.
#
# 3.8, 3.9 and 4.0 gate a walker's arrival line on its PRE-MOVE location not
# being the Runner's "never placed anywhere" zero.  run400 @468A64 is the
# whole test in one line -- ShowEnterExit AND (old <> playerroom) AND
# (old <> 0) -- with run380 @4416F4 and run390 loc_45A99B the same shape.
# 3.7 has no such test at all (run370 @43955E), which is why no 3.7 row moved.
#
# The subtlety is that the Runner spells "not a room" TWO ways and only one of
# them suppresses the line:
#   0      an NPC the game never placed -- StartRoom 0, sitting nowhere until
#          its walk first fires.  Its first arrival prints NOTHING.
#   &HFF   an NPC a walk's Hidden stop has just hidden (run400 loc_468D4A).
#          It passes the <> 0 test, so its next arrival DOES print -- just
#          directionless, wherefrom() bailing out on the &HFF.
# Scarier stores location as room+1 with 0 for both, so scr_npcstate_t gained
# a `walk_hidden` flag to carry the half of the distinction the announcement
# needs; gs_set_npc_location() clears it, npc_tick_npc_walk() sets it right
# after stamping a Hidden stop.  It is deliberately NOT in the .tas stream:
# the Runner's own save writes a room byte in 0..NumRooms, so a restored
# hidden walker reads back as never-placed at either engine.
#
# Measured live under Wine at three generations -- 3.7 needed none, being
# exempt, and arlo did not move:
#   3.8  tra.taf / run380 / Adven_9_timmy_reid.rtf -- the Runner does NOT print "Sting
#        walks towards you." when the tattooist first appears.  It DOES print
#        "Canadian couple walks towards you from the north." for an NPC that
#        had been placed, so the gate is the zero and not the walk.
#   3.9  S_Tar_Dus.taf / run390 / Adrift_38_stardust.txt (117 commands, full replay) --
#        the strongest single measurement in this round: all 129 walk lines
#        match count for count across four walkers and six directions, and the
#        one line the gate removes, a bare "Plant Lady prances along.", is
#        absent from the Runner while its four directional siblings are
#        present in both.
#   4.0  Orient_Express.taf / run400 / Adrift_36_orient_express.txt -- "Gimme Atip enters."
#        is printed by Scarier and by no Runner.  This was the divergence that
#        started the item.
#
# Everything the gate removed is a first-ever arrival of a never-placed NPC,
# one to three lines per game, and no game lost a *directional* arrival --
# which is the shape you would expect and a useful check if this is ever
# revisited.
#
# 2026-08-25, off the SAME transcript and its xfiles twin: an EMPTY command
# line -- a bare Return -- is a parser complaint too, not silence.
# `cmdfile_stardust.txt` and `cmdfile_xfiles.txt` are the only CRLF feeds in
# the Wine harness, so every command in them went in followed by an extra
# empty Return, and both Runners answered every one: run390 with S_Tar_Dus's
# own ALR for DontUnderstand ("I are confused.  DURHH!", 115 of them in
# Adrift_38_stardust.txt), run400 with xfiles' ("Nope!", 22 in
# Adrift_31_xfiles.txt).  Nothing follows the message -- no walk line, no
# event line -- so an empty command does not tick the turn either, which
# falls out of the complaint returning FALSE (the same rule the Diary of a
# Stripper note below pins for gibberish).  Upstream SCARE guarded the
# complaint with `if (!scr_strempty (command))`; that guard is gone.  38
# goldens gained one to 45 lines apiece, all purely added and no route moved
# -- a blank line in a solution file is a turn like any other, and the ones
# that are only layout now say so out loud.
#
# NOT ported, and this row is where it bites: with the two above set aside and
# the Verbose brief-mode headings discounted, the 117-command run390 replay
# holds exactly ONE engine divergence left, and it decides the game.  cmd 38
# `take needle box` -- obj3 Short `needle`, obj12 Short `needle box`, both on
# the desk, the needle already taken -- answers "You've already got the sharp
# needle!" in run390 and takes the box here: the Runner resolves a reference to
# the LOWER-NUMBERED object whose Short is merely contained in the command, not
# to the longest match (the 3.90 twin of the run400 asteroid_after measurement).
# Without the box, cmd 99 `put needle in box` fails TASK 4's first restriction,
# so the `sw` at cmd 116 misses ending T35 and the real Runner falls through to
# T36, the LOSS.  This golden's win is therefore Scarier-only.  When the
# reference-resolution port lands, the route's repair is `take box` (obj12's own
# alias; verified offline to still reach T35).  Write-up:
# notes/WINE-TRANSCRIPTS-TODO.md, "run390 is not longest-match either".
stardust_solution.txt|S_Tar_Dus.taf|You decide to go with the plant lady and
# Diary of a Stripper is AIF: the game's text is sexually explicit, so its
# solution and golden are deliberately NOT committed (they are in .gitignore).
# The row stays so the regression runs where the files exist; elsewhere it
# NOSCRIPTs.  Mechanically it is worth keeping for three reasons: the score is
# an ALR table (WINTEXT prints `[end=%win%]` and fourteen ALR entries rewrite
# it, so the 13/13 maximum is provable from the file rather than assumed); the
# clock is two chained events, one starting the other; and it pins the rule
# that a command the parser REJECTS does not advance the turn counter, so
# gibberish padding never makes a timed event fire.
diarystrip_solution.txt|diarystrip.taf|You earn a huge tip and the ladies are all in love with you
# Silk Noil: the author's own 10-command walkthrough (shipped in sn_zip.zip,
# kept as downloaded/SilkNoil_walkthrough.txt) replayed verbatim.  No score at
# all -- the game's `score` task says so outright -- so "Congratulations!" is
# the whole goal.  Worth a row because it is the corpus's cleanest example of
# a STAGED COUNTER: `get key` x4 and `pull bolt` x4 are each four
# non-repeatable tasks sharing one command, chained on a variable
# (`RESTR type=4` on the value its predecessor wrote), so the Nth identical
# command matches the Nth task and only the fourth advances the plot.  Its
# other 250-odd tasks are custom refusals, not puzzles.
silk_noil_solution.txt|SILKNOIL.TAF|The Silk King sprays his crotch liberally with a perfume that soon befouls
# The Wheels Must Turn: again the author's own walkthrough (zip_w105/
# walkthru.txt), replayed line for line.  No score anywhere in the file and
# exactly one EndGame action, T41 `cut * 23 *`, so the route is maximal by
# construction.  Two reasons to keep the row.  (1) It is a pure fixed-clock
# game: events 5-14 all have startTask=14 (1-based -> T13 `read book`) and
# fire on absolute offsets +1..+8 from it, so the six conversation turns are
# timing, not content -- seven `z` replay identically -- and the clippers drop
# in turn +7's event phase, which makes `take tool` the +8th command.  (2) The
# ending is invisible from the engine side: T41 is an `ACT type=6 v1=1`, so
# "Better luck next time." IS printed, and the game's own ALR table rewrites
# it to two spaces (as it does the score line, the percentage line and the
# press-any-key line).  A game can ALR every stock Runner message out of
# existence, so a win marker must come from the game's own text.
wheels_must_turn_solution.txt|Wheel105.taf|That is it, Twenty-Three.|SCR_SKIP_WAITKEY=1
# Asylum: no score, no events, and a strictly linear gate chain -- screwdriver
# -> unplug the TV (frees the office door) -> break Dr. Walsh's chair (frees
# the cabinets and the intercom) -> intercom option 2 (frees the pills) ->
# cross picks the padlock -> pills in the guard's coffee (frees `d`).  Two
# things make it worth a row.  Its conversation menus are ROOM-SCOPED BARE
# NUMBERS whose meaning flips on task state -- `1` in the office is T25 while
# `push button` is undone and T36 once it is done -- which is a v4 idiom the
# corpus was light on.  And its WINTEXT is "<br><br>": non-empty, so the
# engine prints no "Congratulations!" and the whole ending is task text, the
# mirror image of the ALR trick in the Wheels row above.  Ends on `asylum`,
# the loop the title is about; `reality` is the other, equally-won ending.
asylum_solution.txt|as.taf|A large plaque sat on the wall|SCR_SKIP_WAITKEY=1
# Life: UNFINISHABLE, and wired the way `hangover` and `penrhyn` are -- as a
# demonstration route, not a win.  There is not one `ACT type=6` in the file
# and not one `ACT type=4`, so the game has no ending and no score at all;
# the title screen's "get a job, get a girl/guy and get rich" is three
# systems that were never written, and VAR 22 `money` is decremented by two
# tasks and incremented by none.  The row covers every implemented verb in
# one pass: the four house chores, the three `in`/`out` shops (all gated on
# VAR 8 `hour`, all open at the 11:06 Sunday start), the four purchases, and
# the cat.  Kept because it exercises a v4 shape the corpus is otherwise thin
# on -- a pure variable-driven life sim with a 25-event clock/decay mesh and
# no task graph -- and because the marker is itself the point: `stats` prints
# `Health=%health%`, an unresolved reference to a variable the game never
# declares.  `piss` is on the route deliberately: T24 runs its two actions
# and has empty completion text, so a working command reports "I don't
# understand what you mean!".
life_solution.txt|life.taf|Health=%health%|SCR_SKIP_WAITKEY=1
# Renuntio: the corpus's first SPANISH v4 game (ifarchive adrift/spanish/).
# One EndGame action, no score, no variables, and the row is worth having for
# three things.  It exercises the `SYNONYM` table on a non-English game --
# adelante/atras/derecha/izquierda onto n/s/e/w and, crucially, `o` (oeste)
# onto `w`, so a bare `o` is a movement command for the whole run.  Its one
# mechanical puzzle is an object carried IN THE MOUTH: `coger agua` and
# `coger agua con las manos` are written as failures and only `coger agua con
# la boca` moves the object, with T35 restricted on Agua NOT held and T36-38
# on Agua held, three times round a duplicated fountain-and-machine room.
# And the opening gate is `c`, prompted only by the timed event 0 [Luz] that
# fires four commands in with "PULSA C PARA CONTINUAR"; T3 is unrestricted so
# the route could skip the wait, and deliberately does not.  Marker is pure
# ASCII out of the (non-empty, so "Congratulations!"-suppressing) WINTEXT.
renuntio_solution.txt|Renuntio.taf|Yo-nos me alzo y estiendo mis-nos brazos|SCR_SKIP_WAITKEY=1
# House Of Horror: a nine-treasure haunted-house crawl that WINS at 145/155,
# and the missing 10 are a provable author bug rather than a route failure.
# T101-T109 each score one treasure with `RESTR type=0 v2=0 v3=36` (in room
# 35, "Home Free!") -- except T109, the doubloons, whose v3 is 0, and the
# `var3 == 0` arm of case 0/6 in `restr_object_in_place` tests OBJ_HIDDEN, not
# a room.  The doubloons start hidden and T81's zombie shot puts them ON the
# corpse, so from the moment they can be taken the test can never pass; not
# shooting the zombie keeps them hidden and scores the +10 but forfeits T81's
# +20.  Worth a row for three engine shapes.  The ending is delivered by
# `ACT type=0 v1=0` (move ALL HELD) in T110 `drive` plus nine `starter=3`
# events on a one-turn delay, so the score lands on the turn AFTER the ending
# command and T120 `Finish` fires alongside it -- and `score`/`i` are meta
# commands that do not tick, which is why the route's `score` still reads 65.
# It has the corpus's clearest one-shot inventory-scatter trap: T48 `GHOST
# MOVING STUFF` is `rep=0`, gated on the ghost sharing your room, and moves
# ALL HELD to a random room of group 1, so the route deliberately meets the
# ghost empty-handed on turn 7 to spend it.  And it is a good verb-shadowing
# case: `* fire * blunderbuss *` matches seven tasks, of which the two in room
# 11 have no NPC restriction at all and silently burn the single loaded shot.
hhorror_solution.txt|hhorror.taf|It has been a long and frightful night|SCR_SKIP_WAITKEY=1
# Where Is Richard?: a 1000/1000 win in 68 commands, and the corpus's cleanest
# witness for the one-level container nesting in "held by the player".  The
# cupcake that kills the spider is inside the backpack, the backpack was
# inside a closet, and the route never opens or empties either -- it carries
# the pack and `give cupcake to spider` fires anyway, because T12's
# `RESTR type=0 v2=1` follows a carried container down one level (the rule
# probe `p39held` pinned in the real run390 and that Cursed depends on).
# Two more shapes worth the row.  Its one timer is a `starter=3` event with
# `time1=time2=8` hung off the password task, so the computer's VAR 0 walks
# 0->1->2 by command and 2->3 only after eight turns have passed; the
# coordinates that open the transfer booth are refused until then, and the
# route spends the wait looting the house.  And every obstacle has three or
# four alternative solutions all funnelled into one `# ...` bookkeeping task
# by zero-delay events -- fire: mat / water / dirt; spider: pistol / pick /
# cupcake; goo: berries / cupcake / water -- with a single cupcake shared
# between two of them, so the route's choice of cupcake-for-spider and
# berries-for-goo is what lets it skip the key, pistol, ammo and pick
# entirely.  `eat cupcake` is the trap: it poisons you (VAR 5 Health) and
# EVENT 10 keeps incrementing it, which locks out the washer, the rope, the
# pail and the pick.
# Re-blessed 2026-08-25, fourteen lines: the empty room description recorded on
# the yeh_solution.txt row above.  These two are the corpus's only exposure.
richard_solution.txt|Richard.taf|You scored 1000 out of the maximum 1000!|SCR_SKIP_WAITKEY=1
# Camp Windy Lake : Part 2 is AIF (Christopher Cole again, the author of
# diarystrip.taf), so like Diary of a Stripper and Archie's Birthday its
# solution and golden are deliberately NOT committed -- they are in
# ../.gitignore, and the row NOSCRIPTs where they don't exist.  WIN 150/150
# in 146 commands, and the maximum is provable rather than assumed: the game
# has 54 `ACT type=4` score actions and no other award, 3x1 + 26x2 + 17x3 +
# 1x4 + 6x5 + 1x10 = 150 = its own declared maximum, and the route fires all
# 54.  Two `ACT type=6` EndGames, T196 `attack tim` (v1=2, the death) and
# T198 `attack tim * machete` (v1=0, the win).
#
# Kept for two shapes.  (1) A FIXED-CLOCK EVENT AS A GATE: EVENT 0 [skinny
# dip] is `starter=3 startTask=117 time1=time2=5`, and until it fires Laura
# is at the beach and her office is shut, so `in` at the main cabin is
# refused -- the route pays exactly five turns (two `ask laura about ...`
# plus three `z`) and the whole Laura scene, 24 points, hangs off getting
# that count right.  (2) TWO TASKS BEHIND ONE DOOR: the shed's unlock
# (`unlock * shed` / `unlock * lock` / `use * key`, +5) and its enter
# (`open * shed *` / `enter` / `in` / `open * door *`, whose fail text is
# "It's locked!") are separate tasks, so the published walkthrough's single
# `open door` cannot reach the score and the solution spells both out.
# Re-blessed 2026-08-24 for the empty-M1 room-alt start rule; the measurement
# that justifies it is on the lair-of-the-cybercow rows above.
windy2_solution.txt|windy2.taf|You spin and see Liz running out of the woods towards you.
# Salutations (Lumin, Ectocomp 2008) is the smallest 4.00 file in the corpus
# and a one-room speed-IF: 17 tasks, 2 events, no score at all, so the marker
# is WINTEXT prose.  WIN in 10 commands -- jacket, leaves, stick, pack, knife,
# kill the spider, then beat EVENT 1 [spider dead] (`time1=time2=6`) with
# whiskey / pour / burn before the egg sack hatches.  `cut sack`, the obvious
# thing to do with a knife, is the losing EndGame.
#
# Three reasons this row is worth its size.  (1) WAITKEY SHIFT: the intro ends
# in `<waitkey>`, so without SCR_SKIP_WAITKEY=1 the first command is eaten as
# the keypress -- and because of the slips below the game still WINS that way,
# which would have blessed a silently shifted transcript instead of failing.
# (2) `WaitTurns` = 3, so one `z` spends half the six-turn deadline: the
# measured cliff is five ordinary commands alive / six dead, but two `z` dead.
# (3) MESSAGELESS RESTRICTIONS FALL THROUGH: T4 `get knife*` is gated on
# holding the pack and T2 `get stick` on the jacket, but neither restriction
# carries a failure message, so both drop to the library take, which reaches
# into the pack on the ground -- a six-command win exists.  The route takes
# the intended path instead; see the header of the solution file.
salutations_solution.txt|salutations.taf|you'll decline to answer.|SCR_SKIP_WAITKEY=1

# A Day at the Iachini House (Michael Iachini, 2001) is a 27-room chore game:
# the family is out, you hold a to-do list, and its six items are the puzzle
# chain -- fix the basement step, balance the hot tub, wash and dry the
# afghan, lay a fire, shower, then find the remote and watch TV.  WIN with a
# full 115 out of 115 in 170 commands, so the marker is the score line's
# companion, the first line of WINTEXT.
#
# The file's `ACT type=4` tasks total 140, not 115, and the 25-point gap is
# the interesting part.  T51/T52/T53 are three copies of `take * shower *`
# worth 10 each, but every one destroys the only towel and hangs a wet one on
# a bar that nothing ever dries, so exactly one can ever fire (-20); T53 even
# repeats T51's room 16 while hanging its towel in the basement bathroom, so
# its room field is a typo.  And T45 (+5 for landing the hot tub on pH 7 from
# below) is unreachable (-5): the tub starts at 10, T48 scores on the way down
# through 8, and at 7 both `add acid` and `add base` hit T47/T44 first, whose
# now-failing restrictions carry failure messages -- so the scan stops there
# and the later, passing T49/T45 are never consulted.  That is the v4
# first-match rule doing exactly what the Salutations row documents, only here
# it costs points instead of granting them.
#
# Two parser notes the route depends on: `put sheets in dryer` silently falls
# through to the library (T18's `put * sheet * dryer` matches whole words, so
# the plural misses) and quietly strands the run one task short of the dryer,
# and the remote lands *inside* the closed piano, so `push key 80` has to be
# followed by `open piano`.
iachini_solution.txt|iachini.taf|You settle down in front of the TV.
# La hija del relojero ("Nano", Spanish, 4.00) is the smallest 4.00 file left
# after Salutations: ONE room, 8 tasks, 12 objects, no NPCs, and no score at
# all -- `score` answers "Your puntos is 0 fuera of a maximum of 0", the
# author having translated the score nouns but not the frame.  A clockmaker
# watches his daughter die of the roses growing out of her back; the win is to
# wind up the brass Phoenix he made her and let it sing her to sleep.  Eleven
# commands, and the marker is a line of WINTEXT.
#
# Three of the eight tasks are dead, all three in the file rather than in us:
# T6 `*vaso*` and T7 `*Tamborilero*` are `Where` Type 0, runnable in no room
# at all (the Hangover rule), and T4 `Abrir ventana` is unmatchable because
# the game's own `SYNONYM [abrir] -> [Open]` rewrites the verb before the
# matcher sees it -- so the command arrives as "Open ventana" and falls
# through to the library's "Open what?".  The same substitution is what makes
# `hablar hija` work (`SYNONYM [Hija] -> [Maria]` -> T0's `hablar maria`
# alternate), which is how it was pinned: `SCR_TRACE_MATCH=1` echoes the
# post-substitution input.
relojero_solution.txt|relojero.taf|Cierro los ojos y lloro.
# Veteran Knowledge (Robert Street, 4.00, 43 rooms / 359 tasks / 83 objects /
# 15 NPCs / 38 events) is the full-length rewrite of Veteran Experience, which
# is already wired two dozen rows above as `veteran_solution.txt` -- same
# author, same washed-up wrestler, same crowbar, but a whole town in front of
# the arena.  WIN 50/50 in 120 commands, and 50 is provably the ceiling: the
# file's eight `ACT type=4` awards are 2 + 8 + 10 + 4x3 + 8 + 10 = 50 and the
# route fires every one of them.  WINTEXT is EMPTY, so the marker is the ring
# announcement out of T260's own text.
#
# NO PUBLISHED WALKTHROUGH -- David Welbourn covered the earlier game only
# (plover.net/~davidw/sol/adr2nd3hr04.html#veteran).  It did not matter,
# because THE GAME SHIPS ITS OWN HINT SYSTEM AND `SCR_DUMP_TASKS` PRINTS IT:
# the per-task `HINTQ=`/`HINT1=`/`HINT2=` fields are the author's walkthrough,
# one entry per puzzle.  (Typing `hint` in play works too, but it prompts
# `[Y/N]` and echoes its prompt twice, so it is no use inside a golden.)
# Worth remembering for any other game whose author built a hint menu.
#
# Kept for four timing shapes, each of which cost a replay to find.
#   (1) EVENT 1 [Flyer arrives] is `start=4..4`, so the flyer is not there to
#       take until the end of turn 3 -- the three `z`s at the top are the wait.
#   (2) THE PARK IS EMPTY UNTIL THE BEER IS DRUGGED.  `talk to brats` at the
#       north end does nothing and waiting does not help; what moves NPCs 4/5
#       into room 18 is T57 `east` out of the bar, gated on T54 `put pills in
#       beer`.  The route therefore crosses town twice by necessity.
#   (3) `LOOK UNDER RING` IS TWO TASKS AND THE ORDER IS FORCED: T126 (tacks +
#       ladder) needs T138 `throw *acid*youth` NOT complete, T127 (steel chair
#       + crowbar + fire extinguisher) needs it complete.  Ringside before the
#       acid for the High Flyer's props, ringside again after it for the
#       crowbar; backwards and the High Flyer cannot be beaten.
#   (4) THE TITLE MATCH IS ON A CLOCK.  The acid teleports you into the ring
#       EMPTY-HANDED and starts `star attack 1..8` (turns 3-4, 7-8 ... 29-30)
#       plus `star gets out chain` at 36; `spray star` sets VAR 0 `blinded`,
#       which `EVENT 14..20 [unblind star]` clear 4 turns later, and T260
#       needs `blinded == 1`.  The seven-command finish wins on match turn 7.
# Plus one dead end that reads like a loss and is not: levering the crate
# (T86) gets you mugged by the Evil Twins and dumped unconscious in the
# Mysterious room.  That IS the way in -- `touch east wall` for a hairpin,
# then `west`.
vetknow_solution.txt|vetknow.taf|AND THE NEW WORLD CHAMPION IS|SCR_SKIP_WAITKEY=1
# ... and vetknow2.taf is the SAME GAME again.  A zlib-decompress + `strings`
# diff of the two files finds exactly three changed strings -- the author
# byte-field (`Robert Rafgon` -> `Robert Street`), one added sentence in the
# ABOUT text saying so, and the build date -- and not one changed room, task,
# object, NPC or event.  So the two solutions are the same 120 commands and
# the two goldens are byte-identical, which is the point of carrying the
# second row: if these ever diverge, something is reading the header when it
# should not be.
vetknow2_solution.txt|vetknow2.taf|AND THE NEW WORLD CHAMPION IS|SCR_SKIP_WAITKEY=1
# The Lost Tomb (3.90, English, 56,336 bytes; 19 rooms / 99 tasks / 86 objects
# / 1 NPC / 13 events / 6 variables) is the smallest unwired file left.  An
# Egyptology farce: you have found pharaoh Erick's tomb and your funder, Lord
# Rupert Mongoose -- monocle, pith helmet, alarm clock set for tiffin -- has
# invited himself along.  WIN 175/175 in 105 commands, and 175 is provably the
# ceiling: the file's 23 `ACT type=4` awards sum to exactly 175 and the route
# fires every one.  WINTEXT is the trek back to camp, which is the marker.
#
# NO PUBLISHED WALKTHROUGH, and again it did not matter, because THE GAME
# SHIPS ITS OWN HINT SYSTEM AND `SCR_DUMP_TASKS` PRINTS IT -- the per-task
# HINTQ=/HINT1=/HINT2= fields, exactly as with Veteran Knowledge two rows up.
# Second data point for that trick; check for those fields before deriving a
# route the hard way.
#
# One engine-semantics find is why this row is worth keeping: T45/T46/T47
# ("look at the wall" through the death mask) restrict on `type=0 v2=2`, which
# is WORN BY THE PLAYER, not held (screstrs.cpp restr_object_in_place case 2),
# and T44 is the v2=8 "not worn" counterpart that prints a plain, plausible
# wall description.  Carrying the finished mask into the Riddle Room therefore
# reads like success and silently costs the file's single biggest award (+20).
# `wear mask` is the whole difference between 155 and 175.
#
# Four timing shapes, each of which cost a replay:
#   (1) The dynamite must be IN the crack before it is lit.  T59 (lit in hand)
#       starts EVENT 9 -> T61 "BOOM! YOURE DEAD!" two turns later; T60 (+5) is
#       the in-the-wall version.  The crack is hidden until `x walls`, and the
#       object it becomes is named "hole" -- `put dynamite in crack` misses.
#   (2) EVENT 8 [TIFFIN TIME!] is `start=50..80`: when Rupert's alarm clock
#       goes off it zeroes VAR 0 [Rupert] for three turns and every
#       `ask rupert to ...` task is gated on it.  A swallowed request answers
#       with flavour ("the lid is too heavy for you"), not a refusal, so it
#       reads as a wrong solution.  The second sarcophagus is asked twice.
#   (3) The Wall Room is on a clock: EVENT 2 closes the walls one step every
#       two turns (12 -> 10 -> 8 -> 6 feet) and `jam spear in walls` is
#       refused until 6 feet, i.e. turn 7 after the hand-in-the-hole ask --
#       hence the nine `z`s, then two more to bend the spear and open the door.
#   (4) EVENT 6 [PILLAR CHECK] runs T30 at the END of the turn the fourth
#       statue lands, so the ruby it reveals cannot be taken until the turn
#       after.  Without the bare `z`, `take ruby` does nothing AND SAYS
#       NOTHING, and the mask can never be completed -- 155/175, silently.
# Plus one losing ending that looks like the obvious move: climbing out of the
# well while holding the death mask is T33, `ACT type=6 v1=1`.  The mask goes
# up inside the rucksack tied to the rope (T32) with Rupert winding (T36).
losttomb_solution.txt|losttombv2.taf|you and Rupert start the trek back to camp.
# The Long Journey Home (Danny Chabino, 20 June 2001) is UNFINISHABLE, and the
# row is anchored on the score line for the same reason The Hangover's is.
# You wake in your bedroom, step through the bathroom mirror and descend a well
# into an underworld of rooms called Sorrow, Despaire, Anger, Rage, Fear and
# Terror.  46 commands reach 30 of a declared 90, which is the ceiling of
# legitimate play.  Three independent walls, all provable from the dump:
#   (1) 90 IS TWO CAREERS, NOT ONE.  Ten `ACT type=4` awards sum to 90, but
#       T10/T11, T24/T25 and T74/T75 are male/female twins and the game makes
#       you pick on move one.  60 is the most anyone could bank -- and the
#       female half is broken: T24 is `where=0` (runnable nowhere), and T75,
#       unlike T74, has no `ACT type=0` to drop the King of Spades, while
#       `EXIT room=20 N gateTask=74` gates the Gnarled Woods' only way back on
#       T74 specifically.  A woman who enters Terror never leaves.  Play male.
#   (2) RAGE IS A ONE-WAY TRAP.  T22 (room 9, restr=0, `#12 turn valve debris
#       here`) and T25 (+10, `#12 release pressure`) carry the identical four
#       patterns, and forward first-match in `run_game_commands_common()` hands
#       every phrasing to the unrestricted T22 forever.  Rage's only exit is
#       `gateTask=25 wantDone=1`, and T17 intercepts `n` while VAR 5 is unset,
#       so boarding the raft in the Reservoir (T13) is a soft-lock.  The author
#       got the same shape right at T20-before-T21 and at T18-before-T19 (where
#       the reverse order would make `remove debris` instantly fatal to
#       everyone), so this is an off-by-one in their task list, not an engine
#       question.  The route never goes south from the Lair.
#   (3) THE CARD GAME HAS NO STARTER AND THE ENDING IS SEALED.  T76 `#6 start
#       card game` has only the author's internal label as Command[0], no
#       ALTCMDs, no event targets it (affTasks 33/85/82/83/1) and the file has
#       ZERO `ACT type=5` actions -- yet T77/T78/T79 all require it done, so
#       rooms 23-25 and T85's +10 are unreachable.  And T86 `#17 the end`, the
#       file's only `ACT type=6 v1=0`, is itself `where=0`: the WINTEXT can
#       never print.  (A real Runner player could type `#6 start card game`
#       literally -- `!goto lair` proves literal patterns fire -- but that only
#       buys 20 points and still stops at T86.)
# Parser notes: the Creature eats your first move in the Lair (T5, one-shot,
# hands over the King of Hearts); the King of Clubs has no `card` alias; the
# Gnarled Woods is an RNG maze (T68 needs VAR 2 == 2, re-rolled every turn by
# EVENT 4) that takes three `left`s under this seed, and T71's `* *e *` steals
# any word ending in "e", so it is `get king of spades` there, not `take`.
journ2_solution.txt|Journ2.taf|Your score is 30 out of a maximum of 90.
# Murder in Great Falls (no author recorded anywhere -- no author byte-field
# in the .taf and none in games.manifest.tsv; released 24 Nov 2001) is a
# three-day police procedural: Donald Wisker is found dead behind the college,
# and on Day 4 you name the killer out of Rick, Ross and Ken.  WIN, 200 of a
# declared 200, in 98 moves.  The file's 32 `ACT type=4` awards sum to exactly
# 200 and the route fires all 32, so the ceiling is both provable and exactly
# reachable -- 22 fives and 10 tens, with no variables in the file at all.
# THE ROW NEEDS SCR_SKIP_WAITKEY=1 and it is not optional: 15 <waitkey> tags,
# 13 of them on this route, and the FIRST one sits between the game's two
# start-up prompts (name, then the Runner gender dialog), so without the
# variable line 2 is eaten and the run never gets past "Please answer "male"
# or "female"."  That is the Far From Home trap one notch worse -- there the
# <waitkey> was in front of the name prompt, here it is between the prompts.
# Structure notes, each of which cost a replay:
#   (1) THE DAYS ARE TASK BOUNDARIES, NOT A CLOCK.  No events, no variables.
#       Day 1 ends when T35 `ask ross about club` runs, Day 2 when T61 `ask
#       ross about will` runs, Day 3 when T63 `ask ken about trey` runs; each
#       carries `ACT type=1` sending the player home, and every "which day is
#       it" test in the file is a restriction or a room ALT on one of those.
#   (2) ALL THREE CLOSERS ARE `where=3`, RUNNABLE ANYWHERE.  `ask ross about
#       club` fires from the player's own living room, Ross absent and never
#       met -- Day 1 can be closed in three moves.  The route walks to his
#       office anyway; that is the authored path and the better transcript.
#   (3) ONE ROOM IS DAY 1 ONLY, ONE IS DAY 2 ONLY.  `EXIT room=9 E gateTask=35
#       wantDone=0` shuts the Photography Classroom when Day 1 ends (nothing
#       there scores), and room 21, Ross's living room, is reachable only via
#       T10 `knock on door`, restricted "T35 done AND T61 NOT done" -- a
#       window exactly one day wide.  The cigarette in its ashtray is the only
#       evidence with a deadline and missing it costs 10 in silence.
#   (4) `Globals/DispFirstRoom` IS FALSE, so the opening Office is never
#       described -- the transcript goes straight from the gender prompt to
#       the first command.  The author's setting, honoured in run_main_loop().
#   (5) T47 `turn on tv` is `where=0`, runnable NOWHERE -- a third corpus
#       witness after The Hangover and La hija del relojero.  It is invisible
#       in play because the library answers "You can't turn that." first, and
#       any output at all suppresses the 3.9 room refusal.  That refusal does
#       fire here: `knock on door` typed in the Office answers "You can't do
#       that here!", a second live witness for the 2026-08-10 port.
# No published walkthrough exists.  The author shipped a hint menu and
# `SCR_DUMP_TASKS` printed its five HINTQ=/HINT1=/HINT2= entries -- but unlike
# Veteran Knowledge's and The Lost Tomb's, THESE HINTS NEVER NAME A COMMAND.
# They are prose ("Dr. Ross might know something, but you'll have to know what
# to ask him"), so they identify the five gates -- the baggies on the couch,
# the receptionist's parcel past the guard, and one per day for the three
# closers -- and nothing else.  Third game running for the HINT2 grep, first
# one where the answer still had to come out of the `cmd=`/`ALTCMD=` patterns.
# Accusing wrong is a losing ending: T64 `accuse rick` and T65 `accuse ross`
# are both `ACT type=6 v1=1`, and only T66 `accuse ken` is `v1=0`.
murder_great_falls_solution.txt|mudergreatfalls.taf|Ken is found guilty of triple homicide.|SCR_SKIP_WAITKEY=1
# The Vampire With A Conscience -- ADRIFT 3.90, Ole Olsen, 63,183 bytes.
# WIN, 100/100, the file's declared maximum, in 57 lines.  You are a vampire
# delegate at a convention in the Oslo Plaza who has 120 minutes (one per turn)
# to arm himself, raise an undead assistant and shoot the grand master before
# the grand master shoots him.
# THE +10 IS DOUBLE-BANKED AND THAT IS WHY THE AWARDS OVERSHOOT.  18 awards
# summing to 110 against a MaxScore of 100: T94 and T95 are the same +10 on the
# same `push 15`, gated on two different prerequisites (the portiere, or the
# two duct `listen`s), and first-match dispatch can only ever run one of them.
# So 100 is both the ceiling and exactly reachable.  This route qualifies
# through T95.
# THREE CLOCKS RUN AT ONCE, and each one is an event rather than a variable
# test: EVENT 2 [Nutriton] kills you 63 turns in unless T54 `drain jon simonsen`
# pauses it, EVENT 7 [JonsEscape] frees your victim 19 turns after that drain
# unless the stick is jammed in the container handles (T62 -> T77's own check),
# and EVENT 8 [RaiseJon] is the 20-turn wait that T64 `open container` needs.
# On top of those, T83 refuses the taxi queue from 23:40 and T119 loses at
# midnight.  The four `wait`s in the middle of the script are EVENT 8's, and
# four is the minimum -- the file sets Globals/WaitTurns to 3, so one `wait`
# is three turns of event clock (and three minutes of the other two).
# RE-DERIVED for the section-10 exclusive event-length roll: EVENT 4
# [CarsAtRingRoute3] holds the file's only ranged Time (10..15), and its
# changed roll re-times Simonsen's random walk to the Bozo -- he now arrives
# ~30 turns later on seed 1, at a fixed absolute turn no route can beat.  The
# script kills those turns at the club (29 x `x girl`, with `buy beer` hoisted
# into the window); the queue then goes in at 23:39, ONE minute inside T83.
# The row needs SCR_SKIP_WAITKEY=1 for the single <waitkey> that ends the intro.
vampire_solution.txt|Vampire.taf|Now you are the most powerful vampire alive.|SCR_SKIP_WAITKEY=1

# The Merry Murders -- ADRIFT 3.90, 69,489 bytes, December 16 2003.  A seven-act
# locked-floor whodunit at the SynTex Christmas party: every act ends with one
# hinge task that prints an act banner, kills the next guest and teleports the
# player back to the Plaza (T0 `open stall`, T9 `n`, T31 `open microwave`,
# T35 `take syringe`, T41 `x message`, T50 `read journal`).  No clocks, no
# variables; the only timer in the file is EVENT 1 [End Battle] on the roof,
# which gives eight turns to use the syringe on Eric before T52 `Die`.
# WIN with the FULL 135/135 -- the file's 20 `ACT type=4` awards sum to exactly
# the declared MaxScore and every one of them is on the critical path, so the
# `score` two lines from the end reads 125 and the winning blow pays the last
# ten.  Two traps: `read paper` is an ALTCMD of the lower-indexed T37
# `read list`, so Max's note must be read as `read piece of paper` or the
# janitor's closet never unlocks; and T46 `n` in the Computer Lab only unlocks
# the archive door, so a second `n` is needed to walk through it.  The row needs
# SCR_SKIP_WAITKEY=1 for the six act-transition <waitkey>s.
merry_murders_solution.txt|Merry_Murders.taf|You scored 135 out of the maximum 135!|SCR_SKIP_WAITKEY=1

# The Woods Are Dark -- ADRIFT 3.90, 71,216 bytes, Cannibal 2003.  A haunted
# cottage in Black Hill: 23 rooms, 82 tasks, no events and no clocks, so the
# whole game is one dependency chain held together by ten variables.  WIN with
# the FULL 100/100 -- the 21 `ACT type=4` awards sum to the declared MaxScore
# and every one is on the critical path.  Three ordering traps: `lift trunk`
# needs trunk==0, so it must precede `open trunk` even though the writing it
# reveals is not read until forty moves later; `look at fireplace` needs
# hearth==2, which only `sit chair` sets, so the chair is load-bearing rather
# than colour; and TASK 10 `bounce ball` teleports the player to the Back Yard,
# so the route back upstairs starts there.  The last Clearing turn is spent on
# a bare `look` because TASK 45 is `[*]` -- any command is consumed by the
# forwarding to the Graves.  One <waitkey> sits in the title text ahead of the
# menu, hence SCR_SKIP_WAITKEY=1.
thewoods_solution.txt|thewoods.taf|You scored 100 out of the maximum 100!|SCR_SKIP_WAITKEY=1

# Captive Universe -- ADRIFT 3.90, 74,568 bytes, after the Harry Harrison novel.
# 62 rooms, 61 tasks, 19 events, no variables.  WIN with the FULL 100/100: nine
# `ACT type=4` awards (8x10 + 1x20) sum to the declared MaxScore and the route
# fires all nine.  The game is one long clock -- walking out of the courtyard
# gate (TASK 11) starts four one-shot arrest/nightfall events at once, which
# fire at exactly turns 8, 18, 18 and 20 and never again, so the first half of
# the route is "climb a tree that appears in no arrest task's WHERE_ROOMS and
# wait".  Two traps: Globals.WaitTurns is 3, so the four `z`s are twelve turns,
# not four; and EVENT 18 [Timedoor] un-finishes TASK 39 one turn after it runs
# (affTask fin=1), so `w` off the ledge must be the very next command after
# `use crowbar` or the steel door slides shut again.  A shorter route exists --
# `use crowbar` then `swim` in room 35 enters the ship through the swamp for
# the same 20 points and skips the grain quest and the rope entirely -- but the
# committed route takes the author's designed path so the regression covers
# both NPCs, the three chained Smith events and the timed door.  No <waitkey>
# in the file, hence no env.
captive_solution.txt|Captive.taf|You scored 100 out of the maximum 100!|

# Adventures of Thumper - Wonder Wombat -- ADRIFT 3.90, 107,200 bytes, Chris
# Tyson 2001-2002.  51 rooms, 131 tasks, 76 objects, 39 NPCs, 39 events, 15
# variables and 441 <waitkey> tags, hence SCR_SKIP_WAITKEY=1.  The file
# contains NOT ONE `ACT type=4`, so the game has no score at all and the
# end-of-game summary reads "You scored 0 out of the maximum 0! ... 100% of the
# game!" for any ending -- the row therefore matches the winning cutscene's
# closing line instead.  WIN via TASK 127 `*note*` in room 0, which is the only
# `ACT type=6 v1=0` on the critical path (the other three are the two survival
# deaths and TASK 46 `win`, an unrestricted author cheat).
# Four meters (bladder, hygiene, smoke, alcohol) each step by one every five
# turns via EVENT 0 -> TASK 1 `#statsdown`, so the two meter deaths sit ~500
# turns away and only two spots matter: TASK 38 sets hygiene to 0 in the
# dumpster/truck/tip and TASK 50 then kills for hygiene 0 with the gas mask
# off, and the mandatory beer binge drives alcohol past 100 to enter Fantasy
# Land (TASK 57) where the titus component is the only copy in the game.  That
# makes the second half of the route TURN-PARITY SENSITIVE: Fantasy Land opens
# at alcohol 100 and closes at 99, so adding or deleting one turn anywhere
# earlier moves which `drink beer` tips over and how long the hangover lasts.
# Three more traps: the swear-off must be LOST once before KARNISHNAR (the word
# from under the shack doormat) is worth $5000, because losing is what sends
# Percy the Possum to the bar and re-opens the arena; `take fooluffultitus
# pills` is refused while `take syndrom pills` works (the built-in take parser
# matches only the object's own prefix words) yet `give fooluffultitus pills to
# fry` is the form that works (task commands match the raw input string, a
# different matcher); and maze rooms 32-41 answer every compass direction with
# TASK 63, a move-to-RANDOM-room action, so the maze cannot be mapped -- the 12
# norths in the route are simply what the seeded harness needs.
wonderwombat_solution.txt|wonderwombat.taf|THUMPER KICKS ASS!!!|SCR_SKIP_WAITKEY=1

# Vardock Bates -- ADRIFT 4.00, 2,928,980 bytes, "Pipo98" v1.0.2, in SPANISH.
# 39 rooms, 68 tasks, 77 objects, 4 NPCs, 4 events, 1 variable.  The file
# contains NOT ONE `ACT type=4` and WINTEXT is empty, so the game has no score
# and no win banner of its own -- the row matches a line of the winning
# cutscene instead, the same way relojero_solution.txt does.  SCR_SKIP_WAITKEY=1
# is for the four chapter-title <waitkey> screens.
# The author writes his task patterns with ENGLISH verbs and Spanish nouns
# ([take]{el}[mechero]) and ships a SYNONYM table that rewrites the player's
# Spanish input, so both languages parse; the library replies are Spanish.  One
# consequence matters for the route: the built-in take handler does NOT accept
# the Spanish article ("coger el revolver" -> "Que quieres coger?"), while
# author tasks do because they spell the article out in a {el} group -- hence
# the bare `coger revolver` / `coger adoquin` / `coger baston` / `coger
# documento` next to `coger el mechero del bolsillo`.
# Two hard timers.  EVENT 0 [Jinetes] starts when you mount the horse in Egypt
# and executes TASK 23 (`--Fin--`, EndGame lose) ten turns later; only TASK 22
# `decir museo` pauses it, and the shortest path from the horse to the taxi is
# eight turns, so there are exactly two turns of slack.  EVENT 3 [Lanzamiento
# de baston] starts on `hablar con jason` on the museum terrace and kills on
# the NEXT turn, so `esquivar el baston` must follow it immediately.
# The revolver is a pure trap: TASK 56 (touch/attack the wolf) and TASK 57
# (shoot it) both `exec task 23`, as does TASK 31 `kill jason`.  The Kork wolf
# is beaten by throwing the cobblestone at it (TASK 58), which is also why the
# adoquin has to be picked back up after it shatters the bathroom mirror.
# The endgame is a two-ending choice in Brasil and BOTH are `ACT type=6 v1=0`
# wins: TASK 36 `poner el brazalete` (go back to being human) and TASK 35
# `lanzar * brazalete *` (take the Committee's offer).  TASK 35 additionally
# requires 36 UNdone plus 37/38/39 done, i.e. the maletin opened and the
# document taken and read -- so the fuller of the two endings is the one wired.
vardock_bates_solution.txt|Vardock Bates.taf|HAS ELEGIDO LA INMORTALIDAD PARA SIEMPRE|SCR_SKIP_WAITKEY=1

# Lara Croft : The Sun Obelisk -- ADRIFT 3.90, Christopher Cole, Fall 2002,
# 148,447 bytes.  35 rooms, 231 tasks, 40 objects, 30 statics, 4 NPCs, 4
# variables, ONE event.  ADULT AIF, cast is adults throughout, so it goes in
# on the Diary of a Stripper / Camp Windy Lake 2 terms: `goldens/croft_solution.txt`
# and its `.expected.txt` are gitignored and this row is the only committed
# artefact, which is why the mechanics live here instead of in a notes file.
# The row NOSCRIPTs where those files do not exist, which is not a failure.
# WIN, 150/150, in 101 commands.  The ceiling is provable rather than
# assumed: 48 `ACT type=4` awards and no other scoring action, 27x2 + 7x3 +
# 13x5 + 1x10 = 150 = the game's own declared maximum, and the route fires
# all 48.  Eleven `ACT type=6`, ten `v1=2` deaths and exactly one `v1=0`
# (TASK 166, the last command of the route).  No <waitkey> in the file at
# all, no name or gender prompt, so the row carries no env -- NO-WAITKEY in
# the audit.
# THE ROW EXISTS FOR THE 3 POINTS THE AUTHOR'S OWN WALKTHROUGH LOSES.  The
# game ships croftwlk.txt (by John <not_jwc@hotmail.com>, kept as
# downloaded/LaraCroft_SunObelisk_walkthrough.txt) annotated with running
# scores all the way to 150, and replaying it verbatim ends at 147.  The
# game is adult AIF, so the strings are schematic here -- V is the verb the
# walkthrough types, V' the verb the tasks are written with, N the shared
# noun; the literal commands are in the gitignored solution file.  The
# culprit is `V N with jade`: the file's `SYNONYM [V] -> [V']` rewrites the
# input BEFORE task matching (relojero's finding, now in an English game),
# so the matcher sees `V' N with jade`, and TASK 117
# `V' N*` -- unrestricted, six indices in front of the task the author
# meant, trailing `*` swallowing the rest of the line -- claims it.  TASK
# 117 is the solo statue action and has already fired for its +2 earlier in
# the scene, so the second hit reprints its message and awards nothing.  The
# +3 is TASK 123, reachable only with a phrasing 117 cannot claim; the route
# uses TASK 123's own `me and jade V' * N*` (typed with V), the medial `*`
# matching zero words.  First-match precedence,
# not an engine divergence -- MEASURED, not inferred from the task indices.
# Replaying all 101 commands in run390.exe desyncs (the picture window steals
# focus), so the shape was reduced to make_39_synprobe.py, which is built
# from the game's own vocabulary and is therefore gitignored alongside the
# solution; on this machine it sits in this directory.  One room,
# SYNONYM [V] -> [V'], TASK 1 `V' N*` (+0) and TASK 2
# `me and jade V' * N*` / `V' * N* with jade` (+3), both
# unrestricted.  run390 and Scarier agree line for line -- `V N with jade`
# fires TASK1 and scores nothing in both, `me and jade V N` fires
# TASK2 and scores 3 in both -- so the published 150 really is unreachable as
# written, in the Runner the walkthrough was written for.
# Route notes: `shoot goon` at the Waterfall is the scoring branch and needs
# the twin Magnums -- the alternative (sliding down the slope in the Thick
# Jungle) skips Strathairn's Camp and 21 of the 150 points, which the
# author's FAQ puts at 67% of the total.  The Hall of Spheres riddle answer
# is typed bare as `tomorrow`, a task command rather than a `say`/`answer`.
# `x wall` + `push plate` in the second cave room is the only source of the
# headdress stone, without which both lower-level switches are inert and the
# waterfall exit never opens.  The altar must be PULLED, not opened (opening
# it is one of the ten deaths).  Jimmy the Neck is unbeatable by design; `x
# jungle` east of him reveals the path round.  The shirt button, the chunk
# of quartz, the Aztec coin and the Lost Cave's hollowed-out rock altar are
# author-confirmed red herrings and every carried item is taken away before
# the Temple regardless of route, so the route ignores all four.
croft_solution.txt|croft.taf|You scored 150 out of the maximum 150!
# Doctor Who and the Vortex of Lust: 150/150, the fourth Cole game here.
# 25 rooms, 209 tasks, 9 NPCs; 50 `ACT type=4` actions summing to exactly the
# declared 150 and this route fires all of them.  Only two `ACT type=6` in the
# whole file -- `shoot dalek` (death) and `replace staff` (the win).  No
# <waitkey>, but the game DOES prompt for a name, so line 1 of the solution is
# `Sam` and the row still needs no env.
# The author ships NO walkthrough, only drwho-score.txt (kept as
# downloaded/DrWho_VortexOfLust_scoresheet.txt), which lists WHAT scores but
# not what unlocks it.  THE ORDER IS THE WHOLE PUZZLE, and it is a single
# chain: three of the six girls have startRoom=-1 and are placed by finishing
# the one before.  (Each girl's chain ends in a "finisher" task; the literal
# commands are in the gitignored solution file.)  Ace's finisher moves Nyssa
# into Your Room, Nyssa's moves
# Sarah into the Lab and Adric into the Library (and gates `tell tegan about
# adric`), Tegan's hands you the picture that is the only way to score
# Adric, Adric pays with the dispenser code `12553m`, and the wine that code
# dispenses is what starts Sarah.  Leela is the exception: she is on the map
# from the start and has no gate, so she is easy to leave for later -- but
# Sarah's finisher (T159) moves characters 2..7 and 9 to room 0 and empties the
# TARDIS, so the Solarium has to be visited BEFORE the Lab.  Getting that
# wrong costs 15 points and prints no refusal at all.
# Per girl an `X sex` variable gates every body task and is always set by
# something non-sexual: `tell peri about temporal breach` (which itself needs
# `ask k9 about temporal energy`, and K9 is in the first corridor), `round 3`
# of the strip darts, `tell tegan about adric`, `give wine to sarah`.
# Two traps the route avoids: seven of Ace's nine scoring tasks require the
# 7th Doctor's hat NOT to be worn (type 0 Var2=8) -- wear it and 12 of her 15
# points go away -- and `shoot dalek` with the blaster rifle is the game's
# only death.  The Dalek is disabled with the sonic screwdriver from the
# Doctor's jacket in the very first room.  The Kitchen's other code,
# `122-663a`, is a task with no actions: a red herring.
dr-who-vortex-lust_solution.txt|dr-who-vortex-lust.taf|You scored 150 out of the maximum 150!
# The Gamma Gals: 150/150, the FIFTH Cole game here.  44 rooms, 304 tasks,
# 10 NPCs, 32 variables; 68 `ACT type=4` actions summing to exactly the
# declared 150 and this route fires all of them, with not one refused
# command in the 182.  No <waitkey> in the deobfuscated body, but the game
# DOES prompt for a name, so line 1 of the solution is `Sam` and the row
# still needs no env.
# The author ships NO walkthrough, only gamma-score.txt (kept as
# downloaded/GammaGals_scoresheet.txt).  It is grouped per girl, and those
# group totals are what pin the route down: Sharron 14, Sharron & Shannon
# 29, Shannon 9, Heather 10, Kelly 17, Christine 11, Laurie 2, Krista 3,
# Krista & Laurie 10, Stacey 35, Other 10.
# The game is adult AIF, so scene commands are named by task number and by
# role below -- a girl's "finisher" is the last scoring task in her chain --
# and the literal strings are in the gitignored solution file.
# STACEY IS A COUNTER, NOT A PLACE.  The win is T292, Stacey's finisher
# (+10, the file's only `ACT type=6 v1=0`), gated on `stacey sex == 7`
# EXACTLY.  Six non-repeatable tasks bump it and they are the other five
# girls' finishers plus the twins: T91 (the twins) +1, T125 (Shannon) +1,
# T182 (Heather) +1, T232 (Kelly) +1, T250 (Christine) +1,
# T252 `tell krista about laurie` +2.  All six must
# fire, so Stacey is necessarily the last scene.
# Per girl an `X sex` variable gates every body task and is always set by
# something non-sexual: `give bracelet sharron` (bracelet behind the
# Downstairs Bathroom toilet), `show bottle to heather`, `light joint`
# (joint under the Party Room couch, lighter on the Front Porch table),
# `tell christine about erin`, `tell laurie about krista`, `tell krista
# about laurie`.
# THE ORDERING TRAP IS SHARRON: all six solo-Sharron scoring tasks carry
# `CHAR Shannon NOT in room with player`, and T63, the last of Sharron's
# solo chain, MOVES SHANNON IN.  Do it early and 14 points vanish with no
# refusal printed.
# `wendy` (T126, +5, Heather's Room, player ALONE) is also the gate on
# `tell kelly about zeke`, i.e. on all 17 Kelly points; Heather only leaves
# her room once the bracelet is handed over, so the bracelet comes first.
# `mix rum and coke` consumes the coke and the glass but NOT the bottle, and
# T211 (+2) is the one scoring task that needs it still in Heather's hands
# at the end of her scene.
gamma_solution.txt|gamma.taf|You scored 150 out of the maximum 150!
# The next three are derived from the games' OWN in-game hint menus, dumped with
# `SCR_DUMP_TASKS=1 harness/scare <game>` and read off the HINTQ/HINT1/HINT2
# fields (see notes/WALKTHROUGH_TODO.md) -- no external walkthrough for any of
# them exists.
#
# Pirate's Plunder!: nine questions, all three tiers filled, and the sledgehammer
# tier gives literal commands ("Cut the brambles.", "Tie the vine to the hook and
# put the grappling hook in the tree."), so the route is the hint table in order.
# Two things the hints do not say: Ichabod has to be at the BOTTOM of the cliff
# before `pull rope` will hoist the chest ("Ye'll have to findeth some waye,
# thing, or person to steady it from ye bottom") -- `call ichabod` on Ye Treasure
# Beach both fetches him and triggers Captain Hookhead's ghost ship -- and the
# cannon is pushed one room per turn along ship -> beach -> marsh -> ruins ->
# cliff, which only works after `cut brambles` has opened the marsh's east path.
# The chest then goes back the same way and `set sail` ends it at 10/10.  The
# riddle scroll's "toward ye end don't celebrate!" is a real trap (TASK 10).
plunder_gargoyle_solution.txt|plunder_gargoyle.taf|Ye scored 10 out of the maximum 10!
# Albert is Lost!: three questions, full three tiers.  The game has no score, so
# the marker is the closing line.  Its two randomised facts are re-rolled off the
# RNG as the game runs, so the route is NOT transplantable -- insert or delete a
# single turn and both move:
#   * which of the four scenery objects changed (walnut tree / buskin' bucket /
#     vendor's trailer / stalls) and therefore hides the silver key.  Ask each
#     worker `about strange`; exactly one reports it.  Under this solution's
#     turn sequence it is the Sketch Artist and the walnut tree.
#   * which worker is the real wizard.  Give the quarter motherload to the wrong
#     one and they hand over a false magic word that dooms Albert; the Fortune
#     Teller's own advice is to test them with `ask X about wizards` first.  The
#     tell is respect, not knowledge -- the impostor sneers ("wizards are dumb"),
#     and the true word is always LOOKFROTHO.  Here it is Rhymin' Simon (who is
#     himself the transformed Albert, so saying the word on his hill does
#     nothing; he is hiding behind the bush at the booth).
# Needs SCR_SKIP_WAITKEY: the two-page intro's keypresses otherwise eat commands.
albert_is_lost_solution.txt|Albert is Lost! An Adventure in Real Life.taf|Tiberius and Albert went home happily|SCR_SKIP_WAITKEY=1
# Target: 23 questions but tier-1 only, because the author deliberately shipped
# no external walkthrough -- target.zip's walkthru.txt says "Each time Target is
# played certain key facts will change; so an external walkthru is not possible.
# The game does include a built-in walkthru."  That built-in one is the `cheat`
# command, which prints the run's actual compass directions but costs 10 of the
# 100 points, so it is used to CONFIRM the derivation, never in the route.
# Under the seeded engine the three drawn facts are fixed at game start (turn-1
# `cheat` already reports them): target south, spare bullet southwest, sniper
# northwest on the Appleton Tower.  Identification is by description only -- the
# paper's "eye operation" + "unusual footwear" pick out the eye-patched man in
# black flip-flops to the south, and the paper's own art-gallery sighting
# (northeast) is flagged unconfirmed and is a decoy.  The tramp is an undercover
# policeman; killing him yields the badge and the police radio, and answering
# `y` on the radio is what reveals the camera hidden on the air conditioning.
# The row opens with `1` to pick "Play the game" out of the title menu.
target_solution.txt|target.taf|You managed to score 100 out of 100.
# The next three are replays of walkthroughs the authors bundled INSIDE the comp
# archives rather than publishing separately, which is why the IFDB harvest never
# saw them (same story as Silk Noil and The Wheels Must Turn).  Copies kept in
# `downloaded/`; all three are followed verbatim, command for command.
#
# Door: `SummerCompGames08.zip` member `games/doordocs/walkthru.txt`, five
# commands, and the whole joke is the puzzle -- "When is a door not a door?  When
# it is a jar!!!"  No score; the marker is the escape line.
door_solution.txt|door.taf|You head south. You have escaped.
# The Marlin Affair: Prologue: `SummerCompGames08.zip` member
# `games/junedocs/june_walkthrough.txt` (the .taf is `junepro.taf` upstream).
# Forty commands, no score, ends on the sequel teaser.  Needs SCR_SKIP_WAITKEY:
# the prose is paged with keypress prompts, and each prompt eats a line of the
# solution file -- four of them here (`look`, `x cabinet`, `x forcefield`, and
# the `s` after `turn off generator`).  Losing that `s` desynchronises every
# later move, and the run then dies on `unscrew bolt with screwench` ("I don't
# understand what you want me to do with the bolts") in a way that reads like a
# walkthrough bug but is only a swallowed movement.
marlin_affair_solution.txt|marlin_affair.taf|The Marlin Affair: Chapter One|SCR_SKIP_WAITKEY=1
# Can It Be All So Simple?: `SummerComp05.zip` member
# `SummerComp05/cibass/Walkthrough.txt`.  Forty commands of which fifteen are
# `wait` -- the game is mostly a timed narrative, and the waits are load-bearing,
# not padding.  No score.  The end is not a victory in any ordinary sense (the
# "monsters" were the narrator's family), and the author signs it off with
# "[Press any key to discontinue]", which is what the marker greps for.  Needs
# SCR_SKIP_WAITKEY for the same reason as the Marlin Affair.
cibass_solution.txt|CIBASS.taf|[Press any key to discontinue]|SCR_SKIP_WAITKEY=1
# Pestilence: Richard Otter's own bundled `Walkthru` (copy in `downloaded/`),
# eighty-five commands replayed verbatim.  The solution opens with `1` to pick
# "Play the game" out of the four-item title menu.  Full marks, and the marker
# pins the score so a silent scoring regression cannot pass.
pestilence_solution.txt|pestilence.taf|You managed to score 100 out of the maximum 100.
# Give Me Your Lunch Money: derived, not replayed.  The author's bundled
# `WALKTHRU` (copy in `downloaded/`) is prose and says so itself -- "Commands
# below will not function if used verbatim, but should be taken as general
# instruction" -- so the sixty-five commands here are its steps turned into real
# input.  Gather the four prank components (fishing line from the box at the
# secret stash, mud from the front yard spigot with the laundry-room bucket, the
# watermelon from the kitchen table filled from the garden hose, the Rare Bears
# underwear from Sis' room), `set up` each on the playground east of the school,
# go to bed, climb the crawl tube, wait out six turns and pull the strings four
# times.  `set up` is the game's own custom verb; ordinary `put`/`drop` will not
# arm a trap.  Needs SCR_SKIP_WAITKEY for the paged intro.  The game's banner is
# `- - - Victory! - - -`, but a marker may not start with `-` (the harness passes
# it straight to `grep -F`), so the marker drops the leading dashes.
gmylm_solution.txt|GMYLM_2010.taf|Victory! - - -|SCR_SKIP_WAITKEY=1
# Provenance: the author's `walkthrough_short.txt` (bundled in provenance.zip,
# copy in `downloaded/`) with two deliberate departures, both forced:
#
#   * Around the china/crystal errand the route waits nineteen turns in the
#     dining room before `get china` / `get crystal`.  TASK 81/87 both carry
#     RESTR type=3 v1=2 v2=0 v3=0 -- "the butler is in the same room as you" --
#     and each adds 1 to the `butlermap` variable; only at butlermap==2 does
#     TASK 422 fire TASK 423 and the butler hand over the map of the caves.  The
#     event that voices his request ("...help moving the fine china and crystal
#     ware") is pure narration and does NOT move him; on the turn it fires he has
#     just walked out ("The butler exits."), so taking the china right then gets
#     the plain library take, butlermap stays 0, and the map is never given.  The
#     author's transcript was recorded on a build where he happened to be present.
#   * The final ferry loads the rugged rucksack instead of carrying by hand.  The
#     author's eleven-item pickup at the maze entrance overruns both carry limits
#     here ("Your hands are full at the moment." on the rucksack, "too heavy" on
#     the binoculars and the raincoat), so the rucksack is taken, worn and opened
#     first and every item goes inside it; at the altar they come back out one at
#     a time.  This also folds the author's second round trip into one, which is
#     why the route is shorter than the source file.
#
# This row once carried SCR_SEED=2: EVENT 7 (immediate, Time1=0 Time2=1) runs
# TASK 124 "#Run Gender Task", whose Where list is rooms [0, 165], while EVENT 8
# moves the player out of room 165.  Only a length roll of 0 -- finishing during
# load, still in room 165 -- gets the brown tweed suit worn, and four live
# run400.exe runs all wore it, but under the old inclusive [Time1, Time2] roll
# seed 1 rolled 1.  The event-length roll is now exclusive of Time2 (measured
# live in run400 AND run390 with `make_arena_probe.py EL` /
# `make_39_evlenprobe.py`, RUNNER_TESTS_TODO.md section 10), so a 0..1 range
# always rolls 0, every seed wears the suit, and the pin came off.
#
# 260/300 is a win, not a shortfall: the readme says outright that "it is possible
# to win the game without scoring all the possible points ... the goal of the game
# is not to score the maximum number of points".  The stray `a cauldron` line is
# the author's own typo, kept verbatim; it is a parse error and costs no turn.
provenance_solution.txt|provenance.taf|Look for PROVENANCE II in the summer of 2006!!!|SCR_SKIP_WAITKEY=1

# Professor Von Witt's Fabulous Flying Machine, from the game's own bundled
# "Professor walkthrough.txt" (annotated transcript).  Replays VERBATIM,
# `pick pretty flowers` included -- pick is a Runner take-synonym
# (library patterns "pick up %objects%" / "pick %objects% up").  Getting here
# surfaced three engine fixes: the pick patterns themselves, the room-alt
# "state of object" Var2 being a 1-based GLOBAL object number (the whole
# Laboratory description lives in two alts keyed on the mailbox-on-a-rope's
# state), and the surface-listing count split ("On the shelves is ..." for
# 3+ objects vs "... is on the shelves." for 1-2, mirroring containers).
# The transcript is a Verbose-ON session (bold room heading + NPC walker
# lines on re-entry; Verbose OFF prints only "RoomName." and no walker
# lines).  One known transcript deviation: on the turn-12 `west` into
# Whimsington Square we print "Shelly is walking slowly, delivering the
# mail." where the author's transcript is bare.  A live run400 measurement
# (fresh session, Verbose ON, parity-flipped so the square entry lands on
# turn 12) shows Shelly IS there on turn 12 -- her deterministic walk parks
# her in the square turns 12-21, exactly our phase -- so the bare line is a
# stitched-transcript artifact, not an engine bug.  The author's later
# turn-19 `east` entry has the Shelly line and matches us verbatim.
# No name prompt, no <waitkey>, so the row needs no env.  Ends the same way
# the author's own walkthrough does -- Burton gets the IOCC board seat --
# 151/229, 65%: this is the walkthrough's intended finish, not a shortfall.
# Re-blessed 2026-08-25, one line (`examine contraption` -> "You can't see the
# flying contraption from here!"): the 4.0 seen-but-absent resolver, measured
# on p4EXAM.taf/run400 vs p39EXAM.taf/run390.  Note the article and the "from
# here!" tail are examine's alone -- open and buy take "the ...", close takes
# the object's own Prefix.  See notes/WINE-TRANSCRIPTS-TODO.md, "FIXED
# 2026-08-25 -- the 4.0 seen-but-absent resolver".
professor_solution.txt|Professor.taf|You scored 151 out of the maximum 229!
# The Wingman (AIF), by Dark Horse, 2011 minicomp.  The game ships its own
# walkthrough.txt (bar-scene command list plus a topic-list of body-part
# verbs for the bedroom scene, warning that two of its scoring commands are
# the game's own documented losing endings), but very little of it replays
# literally: `pay bartender with twenty dollar bill` isn't a recognised verb
# (TASK 0's real command is just `pay bartender`), and `turn stereo on` /
# `undress stacie` aren't recognised either (TASK 17 is `turn on stereo`,
# TASK 23 is `remove dress`).  The real command sequence, and the fact that
# a Condom must be WORN and the player's own pants REMOVED before the
# climax task (TASK 48) will succeed, were derived from
# `SCR_DUMP_TASKS=1`'s task/RESTR dump rather than the walkthrough text.
# The walkthrough lists three winning finishers gated on TASK 48; two of
# the three score 95/121 while the third only scores 75/121, so one of the
# higher-scoring pair is the one wired.  As with the games below, the
# literal commands are in the gitignored solution file.  Adult content,
# cast is adults throughout, so it goes in on the Diary of a Stripper /
# Camp Windy Lake 2 terms:
# `goldens/wingman1_solution.txt` and its `.expected.txt` are gitignored and
# this row is the only committed artefact.  The row NOSCRIPTs where those
# files do not exist, which is not a failure.  The game DOES prompt for a
# name, so line 1 of the solution is `Hero`; no <waitkey>, so the row needs
# no env.
wingman1_solution.txt|wingman1.taf|You scored 95 out of the maximum 121!

# Fourth wave, 2026-08-23-: the manifest grew to 431 rows (issue #119's IFDB
# adrift-4 tag sweep) while the corpus sat at 252 wired -- 179 unwired files,
# smallest-first, vocabulary-scanned for AIF content before deriving.  See
# "Fourth wave" in WALKTHROUGH_TODO.md.
# Newton (1291 bytes, 4.00): joke micro-game, one room, no score. An apple
# falls off a tree on turn 4; `get apple` on the very next command is the
# ONLY winning move (a bare wildcard and `examine apple` both lose).
newton_solution.txt|Newton.taf|u dscvr gravity
# Conversation With A Picture (2257 bytes, 4.00): one room, no score. Sit on
# the bench, ask the talking Picture NPC about "bird" (unlocks "parrot"),
# then ask about "parrot" to fire the win. 3 commands.
picture_solution.txt|Picture.taf|The title of the picture is "The Parrot's Cage".
# Smote (1987 bytes, 4.00): 3 linked worlds (Water/Volcano/Desert), no score.
# Play the god Jimmy: get ice in Water World (smites it), carry it + the
# Desert pyramid to pop the volcano (smites it), melt/carry the ice to
# flood the Desert (smites it, wins). 9 commands.
smote_solution.txt|smote.taf|smote all 3 worlds into submission
# Rift (2606 bytes, 4.00): 3-room unfinished intro/demo, no score. `move` in
# the Steel Room, `x the floorboards` (article required) reveals the Lab and
# moves you there, `x the machine` is the win (author's own "unfinished demo"
# disclaimer as WINTEXT). 3 commands.
rift_solution.txt|rift.taf|Thanks for playing this intro.
# The Foggy Banana Adventure (2745 bytes, 4.00): one room, no score. A strict
# TALK/INSPECT/USE chain -- `use hoover`/`use phone` each consume a generic
# no-restriction task ahead of the real gated one in task order, so both must
# be issued twice. 8 commands.
foggybanana_solution.txt|The Foggy Banana Adventure.taf|SPIDERS have been captured by you and sold to the
# The Vault (4258 bytes, 4.00): post-apocalyptic survival sim, no score. The
# intended quest (cross to the Street, get a key off a dying man, unlock a
# drawer for a bible, return to the Vault) is entirely bypassable -- the
# win task ("read bible" in the starting room) has zero restrictions, so it
# fires with no setup at all. Authoring bug; 1 command.
vault_solution.txt|The Vault.taf|And as if  the gods have answered you, the vault door begins to open.
# Pilfers (3727 bytes, 4.00): two-room escape/logic-puzzle skin, 107/107 max.
# Blue Room quiz answers are flavor-only; DOOR:2 not DOOR:1 (an unconditional
# death trap). Red Room bonus content, then task 18's RestrMask is AND, not
# OR -- both `throw rock at window` and `push bed to window` are required to
# climb out. 16 commands.
pilfers_solution.txt|Pilfers.taf|You scored 107 out of the maximum 107!|SCR_SKIP_WAITKEY=1
# Witness: Demon vs. Vampire (3849 bytes, 4.00): two rooms, no score. The
# game ships its own hint system giving away the solution: matches + oil +
# holy water from a cache, draw a pentagram (traps the demon), kill the
# vampire with holy water, lure the demon east into the pentagram, light a
# match. Order matters -- holy water before the pentagram is a death trap.
# 13 commands.
witnessdemon_solution.txt|Witness_Demon_vs_Vampire.taf|You have saved your church from the horrors of the two monsters fighting over|SCR_SKIP_WAITKEY=1
# The Stowaway (3785 bytes, 4.00): 10000/10000 max. Climb to the crow's
# nest, dialogue a ghostly Strange Kid three times (catching him early is an
# instant death), then during the lightning-storm timed event `use kid as a
# shield` -- counter-intuitively the winning, max-score move is sacrificing
# the kid as a lightning rod. 16 commands.
stowaway_solution.txt|The_Stowaway.taf|Well done - you scored maximum points!
# Blast (3447 bytes, 4.00): Ectocomp-2008 horror mini-game, no score. A
# 100 HP demon roams 7 rooms on a deterministic turn-indexed patrol; four
# weapons are scattered on surfaces (each needs its surface examined before
# `get` works). Killing the demon (100 total damage) sets a hidden "frag"
# flag -- the real unlock for the south exit (a Var1-offset-by-2 restriction
# quirk masked it as a check on an unrelated "body" variable). 25 commands.
blast_solution.txt|blast.taf|You exit the building.  You have won!!!!
# Conversation with a Hitchhiker (4401 bytes, 4.00, Ectocomp 2008): one
# room, no score, three labeled "Ending N of Three" branches. `kill the
# hitchhiker` is an explicit win task (Ending Three) -- far shorter than
# surviving the doom-timer to Ending Two, and equally a clean win since the
# game has no score to maximize. Two leading blank lines page past the
# intro's [MORE] prompts.
hiker_solution.txt|hiker.taf|You have found Ending Three of Three.
# Just Another Day (2886 bytes, 4.00): Groundhog-Day time-loop game, no
# score. A task21 AND gate requires four side-quest flags (pet the wolf,
# take the leaf, visit the old man, an undressed talk with the boss) before
# `Jump` is accepted from Outside; jumping resets into a parallel "empty"
# map whose real exit is `w` from Cubicles (`e` there is a dead-end joke
# room reprint). 135 lines, mostly blank padding for "press any key" pauses.
justanotherday_solution.txt|Just Another Day.taf|Congratulations...You won the game.|SCR_SKIP_WAITKEY=1
# Way Out (4598 bytes, 4.00): 5-room horror vignette, no score. A straight
# corridor north to the exit; optional look-left/-right side commands drain
# a "sanity" variable toward a stop-game threshold but are entirely
# avoidable. 5 lines (a leading "1" answers an opening prompt).
wayout_solution.txt|Way Out.taf|You're alive! But you'll never be the same...
# The Fly Human (9-room linear corridor, 21 tasks, no ChangeScore/EndGame
# actions anywhere in the data): unwinnable/unscoreable by design. The final
# two rooms fire via automatic post-completion events, not player input.
# "Still... I guess this is the end." is authorial flavor text only.
flyhuman_solution.txt|The Fly Human.taf|Still... I guess this is the end.
# zombiecow (5-room comedy, 100/130 max: two mutually-exclusive +30 endings
# both count toward the declared 130 max but only one is ever reachable).
zombiecow_solution.txt|zombiecow.taf|You are a free cow now.|SCR_SKIP_WAITKEY=1
# raccoon (6-room, no scoring): trip yard traps, splice dog's leash onto the
# garbage-can lid cord to yank the guarding rock away during an automatic
# chase sequence, then loop back and open the unguarded can.
raccoon_solution.txt|raccoon.taf|You dive headfirst into the can, easily shredding thin plastic bags with your|SCR_SKIP_WAITKEY=1
# outline (3-room detective puzzle, 5/5): push bookcase to reveal a hidden
# passage, lever a floorboard with a ruler, fill a mug via a pipe to wash
# tweezers, pull a hairpin, pick the office lock; safe combo is in the bin.
outline_solution.txt|outline.taf|Well done - you scored maximum points!
# hungry (Ectocomp 2011, 9-room escape, no scoring): grab the pot from the
# reception desk, head to the north office, smash the window with the pot.
hungry_solution.txt|hungry.taf|Escape. Freedom.
# The Long Barrow (8-room dig/tunnel puzzle, no scoring): dig into the site,
# fetch tools after the first collapse, light a torch, defuse the tunnel air
# timer by digging a dark patch, then pry the final chamber's slab loose.
longbarrow_solution.txt|longbarrow.taf|That'll show 'em (and maybe even bag you a raise).
# Asteroid Aftermath (single-hub satellite-realignment puzzle, no scoring):
# valve toggles silently relocate NPC satellites between camera rooms; a
# specific open/close sequence lands all required satellite groups together.
asteroidafter_solution.txt|asteroid_after.taf|All satellites correctly aligned.|SCR_SKIP_WAITKEY=1
# Existence (IntroComp 2009, 3-room ghost vignette, no scoring): use the fan
# to be sucked through and empowered, then use the pencil to win.
existence_solution.txt|Existence.taf|Congratulations!  You've made it through the ADRIFT IntroComp 2009 version of|SCR_SKIP_WAITKEY=1
# P2P (steeplechase reflex race, 30/30 max): jump the Pine Stand, talk to
# George to spook a blocking rival horse at the Wretched Curve, jump the
# Harlequin Pond log pile, then turn to swerve past Tom's horse.
p2p_solution.txt|P2P.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
# Zack Smackfoot (3-room teaser demo, no scoring, single unconditional stop
# ending): open the penknife, jam it in the cargo door's emergency slot to
# release the jam, then exit the wreck with the briefcase for the better text.
zacksmackfoot_solution.txt|zacksmackfoot.taf|THE END . . . . . for now!|SCR_SKIP_WAITKEY=1
# Boiled Eggs (no scoring, single win ending): pump Louise's dialogue tree
# for the spare-key location and Joe's box, unlock the front door, hide
# under the bed until Joe falls asleep, then take the box and climb out.
boiledeggs_solution.txt|boiled eggs.taf|You summon the willpower to keep the box shut until you get home.
# The Shuffling Room (horror vignette, no scoring): release shoulders before
# hands, feel the dark for a hidden lightswitch (needs "use switch" twice),
# open the revealed stone door, climb up, and join the circle to win.
shufflingroom_solution.txt|The_Shuffling_Room.taf|your powerful discovery.
# The Angel the Devil and the Human (river-crossing puzzle, no scoring):
# never leave the Devil unsupervised with the Angel or the Human; ferry
# them to Heaven one at a time via a "predator conflicts with both" swap.
angeldevilhuman_solution.txt|The Angel the Devil and the Human.taf|Have a peanut.
# herrdoktor (3-room comedy puzzle, no scoring): bait a fishing pole with an
# acorn to lure a squirrel, strap on a jetpack fueled by a de-linted
# sweetroll, then launch it down the well to rescue the trapped girl.
herrdoktor_solution.txt|herrdoktor.taf|Mein tiny jetpack ist ein success!
# Rolling the Dough (drunk sneak-into-bed comedy, 50/50 max, sudden-death
# heavy): shoes off before the creaky stairs, stash them in the bathroom,
# throw the rolling pin out the bedroom window, then lie on the bed.
rollingthedough_solution.txt|rollingthedough.taf|Well done - you scored maximum points!|SCR_SKIP_WAITKEY=1
# MurderMansionntro (3-room promo teaser, no scoring, no win condition): work
# through the intro menu, examine the stoop objects, then bang the knocker to
# reach the demo's fixed closing-credit screen -- the fullest reachable content.
murdermansionntro_solution.txt|MurderMansionntro.taf|Thank you for trying my Intro to Murder Mansion|SCR_SKIP_WAITKEY=1
# Whitterscap's Key (Q-key running-gag comedy, 2/2 max): give the button to
# Charles, decode Brelgan's runes, pick the Zenes spell, steal the key from
# Whitterscap, then type a Q-word for the score bonus before quitting to win.
whitterscap_solution.txt|whitterscap.taf|You win with the best score and stuff, yeah!
# The Dangers of Driving at Night (unscored horror vignette): drive north
# through the accident event, pay the gas station clerk, spare Chris some
# change, refuse trucker Harold's ride, then let Chris reveal the back exit.
dangersdrivingnight_solution.txt|The Dangers of Driving at Night.taf|no longer bothering to hide his long, curved fangs|SCR_SKIP_WAITKEY=1
# All Hallows Eve (3-room Halloween vignette, 23/26 true max -- 3 pts belong
# to a mutually exclusive alternate ending): brew a love potion from toad
# eggs, purple beetles, and bird-bath water, then trap and ransom the cat.
allhallowseve_solution.txt|All Hallows Eve.taf|You scored 23 out of the maximum 26!|SCR_SKIP_WAITKEY=1
# Gorxungula's Curse (unscored surreal fantasy): deliberately die walking west
# to seed a gold coin on restart, trade the tub's tome to Clathering for
# spirits, then offer both the coin and spirits to Elder Moose's tub to win.
gorxungula_solution.txt|gorxungula.taf|Elder Moose rouses from the depths of thought once the offering is in place.|SCR_SKIP_WAITKEY=1
# Attack of Doc Lobster's Mutant Menagerie of Horror (unscored monster-factory
# sim): repeat the scalpel+sprinkles+envenomator+serum combo across 6 named
# species to deterministically push the hidden death counter past 6000.
lobster_solution.txt|lobster.taf|Next: WORLD DOMINATION!
# Business As Usual (unscored museum tidy-up puzzle): wait out the scripted
# NPC thefts through turn 16, then shuttle Book/Lamp/Shoe home one at a time
# (bare noun words get synonym-rewritten to room travel, so use take/drop all).
businessasusual_solution.txt|Business As Usual.taf|You Won, Of Course
# Oh, Human (60/200, escape-room dead-end trap): the ladder/box-on-crate 100pt
# branch is provably unreachable, so drop the electrical device to free the
# light, cut through the walls at theroom==4, and exit through the door.
ohhuman_solution.txt|Oh_Human.taf|Congratulations!  You beat the game!
# Sandy's Lost Doll (1286 bytes, 4.00): 6 rooms, 9 tasks, no score, zero
# declared objects. UNWINNABLE as authored -- the toilet-check win task's
# RESTR type=4 Var1=0 tests the command's referenced NUMBER (not the `mom`
# counter the author meant), which `look in toilet` never supplies, so it
# always fails and the EndGame task never runs. Ceiling: explore all six
# look-in/under vignettes plus both "mom catches you" toilet lines.
sandy_solution.txt|Sandy.taf|You see no such thing.
# Same game, guarding the referenced-number leak that once won it: Scarier's
# own meta commands ("wait 2", "hist 2") match a %number% pattern, and before
# the scr_ref_number_guard in scrunner.c that wrote the game's referenced
# number -- which is exactly what TASK 8's `$number = 2` restriction tests.
# run400 never sets its referenced number outside a %number% pattern expansion
# and this .taf has no wildcard at all, so the third `look in toilet` must
# still be refused after both meta commands.
sandy_meta_number_solution.txt|Sandy.taf|You see no such thing.
# PTGOOD 8*10^23 (2006 minicomp, 4.00, 9 rooms, 2 tasks, no score). Open the
# Front Desk window to unlock a room-exit shortcut, reach Slan's Bench,
# `open vial` to win. 6 commands.
ptgood_solution.txt|competition2006__adrift__ptgood__PTGOOD.taf|You win! Yay!
# Pick up the phone booth and Cry (1372 bytes, 4.00): one room, no score.
# `x me` silently completes the hidden "cried yet" gate, then `take phone
# booth` fires the win (death-flavored text but a type=6 v1=0 EndGame).
# 2 commands; the game's own hinted `cry` first step is a red herring.
phoneb_solution.txt|Phoneb.taf|Committing its final act of mercy
# JINXTRON (2179 bytes, 4.00): one room, no score, no EndGame anywhere --
# a pure dialogue toy (the childhood "jinx" game) with an 11-state VAR1
# conversation machine and a recurring random-word interjection event.
# No win/loss to reach; 7-command demonstrative playthrough.
jinxtron_solution.txt|JINXTRON.taf|You're unjinxed now.
# The same toy played all the way round its loop (2026-08-23).  Jinx (VAR 1)
# is the whole machine: 1 = not yet jinxed, saying anything that is not `jinx`
# jinxes you (2); five more turns of anything walk it to 7 ("Player, Player,
# Player -- you're unjinxed"); at 7 the Random_say event announces "Oh, EDAM,
# by the way." each turn and typing that same word back (tasks 23-33 compare
# the referenced text against the string variable random_say) reaches 8; `jinx`
# then jinxes JINXTRON (9); saying `jinxtron` three times -- the jinxtron_said
# counter -- frees it (10) and it asks "Isn't jinx fun?"; answering anything
# jinxes you again and drops Jinx back to 2.  So the loop closes and there is
# still no ending: zero type-4 (score) and zero type-6 (EndGame) actions in the
# whole file.  The word to echo back is seed-dependent -- it is EDAM under the
# harness's fixed RNG, so this row is only deterministic there.
jinxtron_full_solution.txt|JINXTRON.taf|I'm free!  Bwa hahaha!
# Sixth batch (2026-08-29), smallest-first through the manifest's remaining
# unwired titles, skipping the vocab-flagged `Sex is Mental.taf` for separate
# triage. All eight derived in parallel, one background agent per game, and
# re-blessed centrally through the real harness.
#
# The Skydiver (7631 bytes, 4.00): WON 1000/1000, the true maximum -- eight
# ACT type=4 awards on the taken path sum to exactly 1000, dominated by the
# +900 for `inflate parachute`. TASK13/TASK14 are two identically-scored
# (+15) mutually exclusive quilt-fix branches on the same broken-quilt state;
# lower-indexed TASK13 always wins, so the shoelace route is mandatory to
# keep the yarn for the later `tie yarn to parachute` step. TASK16 (unscrew
# bottle) is a dead author bug -- its restriction wants an object nothing in
# the game ever creates -- invisible because TASK17's RestrMask ORs the yarn
# branch in without it. No score summary prints at all (MaxScore==0 in the
# authored file), so the win marker is the game's own darkly-comic truncated
# closing line. 23 commands.
skydiver_solution.txt|The_Skydiver.taf|I'm almost dea-
# the_road ("The Road Leads to Nowhere", 7903 bytes, 4.00, Hourglass comp):
# no score anywhere in the file (all 32 tasks score=0) -- a single linear
# story, not a point-chase, reaching its one non-death ending. The backpack
# and the fireplace both gate on the object-*seen* model (`x logs` before
# `get pack`; sticks must be `put in fireplace`, not merely held, before
# `light fire` stops answering a holding-shaped refusal). The trapdoor code
# is a four-painting digit cipher revealed only after the fire. Two fixed
# turn-timers gate the endgame -- an 11-turn cabin collapse and an exact
# 8-turn walk-to-realization once on the Road. 42 commands,
# `SCR_SKIP_WAITKEY=1` (three waitkey pauses: injury, collapse, closing).
theroad_solution.txt|the_road.taf|the knowledge of oblivion|SCR_SKIP_WAITKEY=1
# The Perfect Spy (7988 bytes, 4.00): WON 10/10, the true maximum (four
# ACT type=4 awards, all fired). Transformation "done" flags are transient,
# not persistent -- exits gated on "change into mouse" done really mean
# "currently in mouse form", since turning human again unsets the flag via
# its own ACT type=5 -- so the route must re-transform immediately before
# each mouse-only transit. The blue keycard and the guard's-leg climb are
# form-gated in opposite directions (mouse steals the card but can't reach
# the slot to use it), and a lower-vs-higher-index task pair on `go n` from
# "Inside a Hole" (TASK 12 instant death, TASK 13 the real escape) resolves
# correctly once the yarn distraction is done first -- the ordering the
# author intended, not a bug. 19 commands, no env.
perfectspy_solution.txt|The Perfect Spy.taf|Congratulations!  You have successfully escaped from the facility!
# seciden_oddcomp ("Return to the Forest House", Seciden Mencarde, Odd Comp
# 2008, 8019 bytes, 4.00): WON 102/102, the true maximum (six ACT type=4
# awards of 17 each, all fired) -- the GOOD ending. A silent, unconditional
# 17-turn "Beast Kills Susie" timer runs from turn 0 (the Beast is already in
# the Playroom from the start; the timer's "a beast appears" text is
# misleading flavour, not its entrance), and draining its fang after the kill
# is literally the event's own pauseTask, cancelling the doom clock. Two
# silent-fail red herrings: the nail box must be opened before its shells are
# takeable, and the stool must be dropped (not held) before it can be stood
# on. 21 commands, `SCR_SKIP_WAITKEY=1` (two waitkeys, both after the win is
# already decided).
secidenoddcomp_solution.txt|seciden_oddcomp.taf|You scored 102 out of the maximum 102!|SCR_SKIP_WAITKEY=1
# Perspectives (Justahack, 8043 bytes, 4.00): no score anywhere in the file
# (zero ACT type=4 across 14 tasks) -- a four-ending no-score game, and this
# route reaches the richest, the "Negotiation Style Ending" (the same
# convention used for Everything Emanuelle and S Tar Dus T). TASK 10, the
# "Heroic Actions Ending" (get the gun, then attack Jonah), is provably dead:
# TASK 9 has the byte-identical attack pattern with no restriction and a
# lower index, so the engine's first-match scan always intercepts the attack
# regardless of game state -- an entire ending lost to task ordering, the
# same footgun that has cost points elsewhere in the corpus, here costing a
# whole branch. 17 commands, `SCR_SKIP_WAITKEY=1` (intro pagination eats the
# first ~3 commands otherwise, even though the file's only literal
# `<waitkey>` sits in the unreached death text).
perspectives_solution.txt|perspectives.taf|Congratulations, you achieved the Negotiation Style Ending!|SCR_SKIP_WAITKEY=1
# Big City Laundry (8088 bytes, 4.00): WON, no score system at all (zero
# ACT type=4 across 30 tasks) -- the game's one good ending (TASK 26; TASK 28
# is a robbery loss). The in-building washer/dryer cycle is a scripted dead
# end by design (its own text says the washer comes out "out of commission"
# regardless of care), forcing a second trip to the laundromat's industrial
# machine. Real engine-fidelity footgun: the robbery event is a real-time
# window keyed on the back door's open/closed state, not its lock state --
# leaving it unlocked-but-open across a timeskip fires a silent loss with no
# parser warning; closing the door (which unlocking also does) after every
# crossing avoids it. 78 commands, no env.
bigcitylaundry_solution.txt|Big City Laundry.taf|Congratulations!  You've done it.
# Over the Edge (Ren, Hourglass comp, 6 Aug 2006, 8128 bytes, 4.00): a WWI
# shell-shock vignette with no score and no formal EndGame action anywhere in
# the file -- ends by reaching the literal credits-screen task, not a
# win/lose call, so there is nothing to lose, only a route to find. Examining
# things in the Captain's quarters is order-gated (shapes -> captain ->
# something -> pillow) and reveals the game's real subject: the Captain
# smothering his sleeping men. The literal command `open your eyes` (not the
# passive Groundhog-Day loop TASK 0 otherwise runs) is the true awakening,
# teleporting to a second "Quarters <2>" instance where a timed event opens
# the real exit at turn 5. 23 commands, `SCR_SKIP_WAITKEY=1` (title screen
# waitkey eats the first command otherwise).
overtheedge_solution.txt|Over the Edge1.0.taf|the End|SCR_SKIP_WAITKEY=1
# Drinks (8458 bytes, 4.00): WON, no score system at all (zero ACT type=4,
# one ACT type=6 win on "open casket") -- a Victorian post-dinner ghost
# story, one puzzle, one ending. The casket is a dynamic object seated in the
# room from the start but tagged unseen until entry -- another live instance
# of the object-*seen* model, so `go south` must be the very first command.
# Three dial-wheel tasks each pair an increment task with a lower-indexed
# wrap-to-0-at-9 task, a benign use of the "lower index wins" idiom for
# counter cycling rather than a trap; the casket's cipher-paper and stained-
# glass riddles both hand over the same code the dump gives directly, so
# both are red herrings. 18 commands, `SCR_SKIP_WAITKEY=1` (three intro
# waitkeys).
drinks_solution.txt|Drinks.taf|THE END.|SCR_SKIP_WAITKEY=1
# Seventh batch, 2026-08-29 (smallest-first, continuing the sixth batch):
# R2DC ("Return to Dracula's Castle II: Revenge of Dracula's Castle", 8861
# bytes, 4.00, comedy by "Arthur Winslow"): WON 1000000/1000000, the true
# maximum -- `SCR_DUMP_TASKS` shows exactly one ACT type=4 in the whole
# file (TASK 3, `smoke peyote`, +1000000), and it's the only score in the
# game. TASK 24 (`climb ladder`) has no COMPLETE= text at all, so a task
# that actually fires still prints the library's generic verb-failure line
# ("You can't climb that.") -- the inverse of the corpus's usual
# silent-loss shape, a task that *works* while *looking* like a failure;
# score-neutral, sidestepped by using the plain `u` exit instead. 11
# commands, `SCR_SKIP_WAITKEY=1`.
r2dc_solution.txt|R2DC.taf|You scored 1000000 out of the maximum 1000000!|SCR_SKIP_WAITKEY=1
# The Forest House [A Text Adventure Mini-Game, v.2] (Seciden Mencarde,
# 2007 ADRIFT Ectocomp, 9476 bytes, 4.00): WON 13/13, the true maximum --
# eight ACT type=4 awards sum to 13, all reachable and none mutually
# exclusive; the game's own `Globals.MaxScore` is 12, one short of its own
# tasks' actual payout, an authoring bug the engine faithfully reproduces
# ("You scored 13 out of the maximum 12!"). Both endings gate on a single
# `injured` variable set only by the "thorns without stick" branch, so the
# sweater+stick combo (the only scoring thorn-crossing) is also the only
# way to keep the good ending reachable. 34 commands, `SCR_SKIP_WAITKEY=1`.
foresthouse2_solution.txt|TheForestHouse_2.taf|You scored 13 out of the maximum 12!|SCR_SKIP_WAITKEY=1
# The Shetland Enigma (9485 bytes, 4.00): WON, score 210 -- provably the
# ceiling, since `SCR_DUMP_TASKS` shows exactly 18 non-exclusive ACT
# type=4 awards summing to 210, all fired on this route; the game's own
# declared "maximum of 100" undercounts its own scoring total by 110, an
# authoring quirk reproduced verbatim ("You scored 210 out of the maximum
# 100!"). A startup-screen variant of the object-*seen* model: the boot
# room description lists the ice chunk but does not mark it seen, so
# `take ice` as the literal first command fails until an explicit `look`
# re-prints the same text and seeds it. 66 commands, no env.
shetland_solution.txt|The_Shetland_Enigma.taf|You scored 210 out of the maximum 100!
# Take One (Robert Street/"Rafgon", finish-the-game-comp-2005, 9547 bytes,
# 4.00): WON, no score system at all (zero ACT type=4) -- the game's one
# and only ending (one ACT type=6 in the whole file). A demon-arrival timer
# (VAR2 timeleft, +1/turn from TASK 8) must reach exactly 16 while the
# jewel sits caged and the switch is off, routing the demon to eat the
# jewel (TASK 49) rather than catch the player (TASK 17, a `restart` loop,
# not an ending) -- 8 explicit waits land on the threshold. 22 commands,
# `SCR_SKIP_WAITKEY=1`.
takeone_solution.txt|takeone.taf|it only took 1 take|SCR_SKIP_WAITKEY=1
# Tenebrae Semper (Seciden Mencarde, EctoComp 2010 "3 Hours", 9757 bytes,
# 4.00): **unwinnable** -- confirmed, not just unreached. No score system
# exists, and all three authored endings are dead: TASK21 has zero ALTCMD
# entries so it can never match typed input; TASK18/TASK24 both require
# reaching room6, but the sole door there (TASK16, up/north from the
# Science Center hallway) has no ACT entries at all -- its CompleteText
# alone marks the command handled, so the engine never falls through to
# real movement, permanently sealing the only way in. Confirmed against
# `run_all_commands()`/`task_run_task_unrestricted()`: faithful ADRIFT 4
# Runner behaviour, a genuine author defect in the original .taf, not a
# Scarier divergence. Row demonstrates the fullest reachable content
# instead (clock-code lock, inventory-wiping "sit chair" mechanic, the
# notebook subplot) ending on retrieving Lauren's pistol. 34 commands,
# `SCR_SKIP_WAITKEY=1`.
tenebraesemper_solution.txt|TenebraeSemper.taf|You take the loaded pistol from Lauren's dresser.|SCR_SKIP_WAITKEY=1
# Helsing ("Steve Van Helsing: Process Server", 9776 bytes, 4.00): WON, no
# score system (zero ACT type=4), the game's only ending. TASK 13 (`ask *
# about *` wildcard, talk to Frankenstein's monster) and TASK 19 (the
# actual serving of the Wolfman) both silently gate on TASK 11 ("play
# [track] #4", the Rush needle-drop) being done -- a flavor-only jukebox
# command turns out to be a hard prerequisite for two unrelated-looking
# tasks, with no in-game hint connecting them except the song callback in
# the win text. 8 commands, `SCR_SKIP_WAITKEY=1` (long mummy-prologue/
# phone-call intro eats unflagged input).
helsing_solution.txt|Helsing.taf|But not tonight|SCR_SKIP_WAITKEY=1
# The Worst Game In The World... Ever!!! (9858 bytes, 4.00) -- **AIF,
# solution/golden gitignored** (comedic, deliberately misspelled, non-
# explicit adult content between consenting adults; no underage or
# non-consent indicators found on a full-transcript read, the Diary of a
# Stripper terms). No score system and no formal EndGame action anywhere
# (WINTEXT is empty) -- a strict branching tree keyed on a scene-selector
# variable, not a score, so the row takes the richest single reachable
# path rather than enumerating every branch (the S Tar Dus T convention).
# Two engine-faithful bugs found: the game's own SYNONYM table rewrites
# "shoot" to "fire" before task matching, but the intended win task (TASK
# 44, `shoot bomer`) is authored on the literal pre-synonym verb and can
# now never match anything -- permanently dead code that also seals off
# four otherwise-unreachable rooms, since TASK 44 is the only place that
# advances the scene selector past that point; and a copy/paste room-
# mismatch bug turns one of the three sub-branches at that hub into a
# genuine soft-lock (its exit task is erroneously scoped to the sibling
# room). 17 commands, `SCR_SKIP_WAITKEY=1`.
worstgame_solution.txt|WorstGameInTheWorld.taf|Another game then? If you dare?|SCR_SKIP_WAITKEY=1
# Spooked! The Wonders of Science (T.D.S., 9909 bytes, 4.00): WON 8/8, the
# true maximum -- `SCR_DUMP_TASKS` shows exactly four ACT type=4 awards
# (+2 each), all fired on this route, matching the game's own printed
# maximum exactly. Room 6 (the Mad Scientist labs) is genuinely
# unreachable by design, not a bug: both its exits gate on task 12
# `wantDone=0`, but task 12 is the only way into the room that leads there
# at all, and it gates `wantDone=1` -- the ending text explicitly lists it
# as an unanswered episode-2 hook. 34 commands, `SCR_SKIP_WAITKEY=1`.
spooked_solution.txt|Spooked_The_Wonders_of_Science.taf|Congratulations you won!|SCR_SKIP_WAITKEY=1
# video.tape / Video_Tape_Decay.taf (T.D.S., EctoComp three-hour speed-IF,
# 4.00): WON, the game's only ending, no score system (zero ACT type=4).
# T8 `pray`'s ten restrictions all read `v2=4 v3=1`, which resolves to
# container idx 0 = the church bowl -- every one of the ten colored relics
# must be placed INSIDE the bowl, not merely carried; the fail text ("Pray
# harder. Return with more sacred artifacts.") gives no hint of this. The
# unlock combination "1850" is on the cemetery tombstone, not the more
# obvious Cinema note (a decoy). Weight cap is exactly tight: 10 relics at
# weight 9 = 90 = maxwt with zero headroom, so the starting paper and every
# one-time tool (briefcase/knife/shovel/bat) must be dropped as soon as
# used. `kill monster` must be typed bare (`kill monster with bat` misses
# the task pattern). Six room-pairs are gated on `play jukebox` (money from
# the Bank first). 139 commands, `SCR_SKIP_WAITKEY=1`.
videotapedecay_solution.txt|Video_Tape_Decay.taf|And fade to white.|SCR_SKIP_WAITKEY=1
# Regrets.taf (2860 bytes, 4.00): WON, the game's only ending, no score
# system at all (score=0 on every task -- the game ships its own oracle
# walkthrough as flavor text but there is no point total to maximize).
# `SCR_SKIP_WAITKEY=1` is required: without it, a late `...press a key...`
# prompt silently swallows the harness's terminating `quit`, and the
# following `y` gets fed to the game as a real (harmless) command instead
# of confirming the quit, desyncing the transcript. 12 commands.
regrets_solution.txt|Regrets.taf|The game has ended.|SCR_SKIP_WAITKEY=1
# Terrified.taf (Eric T. Dorrath, NaAdWriMo 2007): WON ("The game has ended
# and you have won!"), 60 out of a declared maximum of 65 (92%) -- and 60
# is the true reachable max. TASK89 (the "crossed the fence" +5 bonus) is
# structurally unreachable: it's only invoked via ACT type=5 from the
# west-crossing tasks, but by the time it runs the player's room has
# already been changed by that same task's ACT type=1, so TASK89's own
# `where=1 room=19` check always fails against the *new* room -- a genuine
# authoring bug in the original game, faithfully reproduced. Wearing the
# starting boots on the Gravel Path triggers escalating noise warnings and
# (in one room) instant capture; `remove boots` before that leg. Worn
# objects still satisfy `RESTR type=0 v2=1` ("held"), so burning worn
# clothes needs no separate remove step. Zero `ACT type=6` calls anywhere
# in the file (win/death are done via room-teleport + manual score deltas)
# -- no automatic end-of-game summary, so the route ends with an explicit
# `score`. 57 commands, no env vars.
terrified_solution.txt|Terrified.taf|The game has ended and you have won!|
# Bringing the Rain / rain.taf (3157 bytes, 4.00): WON, the game's only
# ending, no score system. Footguns: one task only matches its pattern a
# full turn after the triggering event, not immediately; an ALTCMD list of
# three candidate strings where only one actually matches at runtime; a
# real inconsistency where `ACT type=0` (move-object) uses a 1-based/
# raw-1 room index while `ACT type=1` (move-character) uses a plain
# 0-based index, in the same file; an item that only becomes takeable in
# an already-visited room after a later trigger event; and a strict,
# no-slack 7-turn "about to be caught" countdown baked into one required
# sequence. 33 commands, `SCR_SKIP_WAITKEY=1` (needed for the win-text
# waitkey).
rain_solution.txt|rain.taf|Lightning bolts away to Thunder's earsplitting roar of triumph.|SCR_SKIP_WAITKEY=1
# howitstarted.taf (2860 bytes, 4.00): WON 6/6, the true maximum (100%) --
# a short, linear prequel/vignette with an explicit fourth-wall-breaking
# non-ending ("And...that's it. Sorry, I mean, I know this isn't the end
# of the story...") that still counts as a formal win via its own
# EndGame/score summary. 29 commands, no env vars.
howitstarted_solution.txt|howitstarted.taf|You scored 6 out of the maximum 6!|
# Station XIII / Station_XIII.taf (sequel to The Shetland Enigma, 4.00):
# WON 200/200 raw points (every one of 14 ACT type=4 awards fires; true
# max, no mutually-exclusive/unreachable awards) via the single WINTEXT
# ending. The declared `Globals.MaxScore` field is a stale 9, so the
# engine happily reports "You scored 200 out of the maximum 9! That is
# 2222% of the game!" -- a genuine authoring bug faithfully reproduced.
# The stepladder is deposited in-room by each of three separate `climb
# ladder` tasks (rooms 9/4/6), so it must be re-fetched and hauled back
# for the second and third climbs rather than carried once. Seven
# distinct object-seen-model surfaces gate key items (table/rack/pool
# table/worktop/worksurface/corpse/workbench). Weight cap is a tight
# wt<=108, hit exactly twice at the route's peak; the flak jacket and
# rifle+backpack (never referenced by any RESTR/ACT) must be shed early.
# Three instant-death traps (TASK27/29/30) and one item-loss trap
# (TASK28, the laser cutter used on the wrong grate) are avoided. 93
# commands, no env vars.
stationxiii_solution.txt|Station_XIII.taf|To be continued...|
# Choose Your Own Three Hour Adventure.taf (100-task branching CYOA, 4.00):
# WON 9/14 -- the per-TASK "score=" field SCR_DUMP_TASKS prints is always 0
# and unused; the real running score lives in a separate variable (`scor`)
# updated via ACT type=3 on individual menu choices, only visible in the
# ending text ("Overall, you got a score of 9 out of a maximum possible
# 14."). A first derivation attempt stalled 1 menu choice short of the
# ending (an invalid out-of-range menu number wasted a turn along the way)
# and was mistaken for a finished win; re-derived from the task graph's two
# path-gating variables (armour picked up in the castle -- needed to
# survive Sophie's gunshot at the climax; a second flag needed to reach the
# genuine `<finished>` room rather than one of the many death rooms).
# `SCR_SKIP_WAITKEY=1` is required: without it a `[MORE]`/wait pause
# silently eats the next menu-choice line, desyncing the transcript into a
# death. 13 commands.
choosethreehour_solution.txt|Choose_Your_Own_Three_Hour_Adventure.taf|Overall, you got a score of 9 out of a maximum possible 14.|SCR_SKIP_WAITKEY=1
# thelasthour.taf (Roberto Grassi, 2004; hate-crime/prison narrative with
# its own content-warning intro screen): WON, the game's only ending, no
# score system. The sole ACT type=6 win is fired purely by a turn-count
# EVENT (`start=120..120`, no other restrictions) -- a first derivation
# attempt landed 2 turns short of the trigger and was mistaken for a
# finished win because the transcript just kept accepting `wait` forever
# with no error; re-verified the exact minimum trailing-wait count needed
# (one short still fails). No sexual/explicit content despite the dark
# subject matter (a scripted execution, racist dialogue, and real
# historical KKK/MLK excerpts) -- proceeds under normal wiring, not AIF
# treatment. 121 commands, no env vars.
thelasthour_solution.txt|thelasthour.taf|"Here we are... MY BROTHER."|
# Sex is Mental.taf (AIF, 8373 bytes, 4.00): comedic explicit content between
# two apparent adults (a psychiatric-ward patient and a nurse), a third
# character's rape threat used only as a narrative danger to avoid, no
# minors -- solution/golden gitignored per the established AIF precedent
# (Diary of a Stripper etc.). No scoring system (MaxScore=0, zero ACT
# type=4 anywhere). The sole ACT type=6 win is a comedic twist ending
# (the "nurse" the next morning turns out to be someone else entirely).
# 33 commands, no env vars.
sexismental_solution.txt|Sex is Mental.taf|Where's that broken Glass?|
# Pete's Punkin Junkinator.taf (4.00): WON 505/575 -- six one-time
# production tasks (soda bottle +200, dragon breath +130, skull of man
# +100, carving knife +75, bat wing +50, eye of newt +20, cow pie -10) sum
# to the declared max of 575, but an internal auto-task ends the game the
# instant a 4th punkin is produced, so only the best 4-of-6 subset is ever
# reachable in one playthrough -- 505 is the true, provably unreachable-
# beyond ceiling, not a missed-content gap. 27 commands, no env vars.
petespunkin_solution.txt|Pete's Punkin Junkinator.taf|You scored 505 out of the maximum 575!|
# The Crooked Estate.taf (8745 bytes, 4.00, Duncan Bowsman): unfinishable
# by design -- a one-room literary/atmospheric horror piece with zero ACT
# type=4 and zero ACT type=6 anywhere in its 58 tasks, and an empty
# WINTEXT. `scream`/`yell`/`shriek`/`laugh` trigger a cascading sequence
# that resets the game's completed-task flags back to the opening state --
# the mechanical embodiment of the game's inescapable-loop theme, not an
# ending. `quit` is overridden by a custom in-fiction refusal task rather
# than the engine's real meta-quit, so termination relies on the harness's
# own EOF-after-quit/y convention like other no-ending rows. Golden is a
# demonstration route exercising every implemented verb/scenery noun once.
# 45 commands, `SCR_SKIP_WAITKEY=1` (an opening waitkey otherwise eats the
# first scripted command).
crookedestate_solution.txt|The Crooked Estate.taf|I quit momentarily, lying motionless, without any will. But, still, something|SCR_SKIP_WAITKEY=1
# Alias Undercover Agent.taf (Alias-TV-tie-in spy game, 4.00): WON 35/35,
# the true and declared maximum (four ACT type=4 awards: +10/+10/+10/+5).
# Object-seen-model gate on the kitchen napkin (not referenceable until
# `examine table` lists it); two distinct, separately-locked grate objects
# (office vent vs. cell-side) needing different verbs (`unscrew` vs.
# `unlock`+`open`); a safe combination that only registers via `examine
# dial` after each `turn dial to N` (turning alone gives no feedback). 41
# lines (name-prompt response + 40 commands), no env vars.
aliasagent_solution.txt|Alias Undercover Agent.taf|You scored 35 out of the maximum 35!|
# A View to a Home.taf (4.00): completed (all three medals collected into
# the trophy case) -- no scoring system at all (MaxScore=0, zero ACT
# type=4 in 93 tasks), so completion is defined by the collection goal,
# not a score. Puzzle chain: bird's-nest key (via stick) opens a locked
# hall closet for the bronze medal; a water-logged kitchen note (readable
# only once a background random event leaves the sink non-full) gives a
# safe combination for the silver medal and a text-maze route for a Rubik's
# cube; solving the cube (button sequence) against a second randomly-
# cycling jacuzzi-water state yields the gold medal. Contains a "young
# girl...suicidal" NPC vignette resolved via a religious book (a hope/
# faith gesture) -- serious/dark theme, no sexual content, proceeds under
# normal wiring per the thelasthour precedent, not AIF treatment. 122
# commands, no env vars.
viewtohome_solution.txt|A View to a Home.taf|Congratulations! You have collected all three medals! You have completed the|
# briefcase.taf (Julius the master-thief, 4.00): WON, the game's only
# ending, no scoring system (zero ACT type=4). A tight two-hidden-event
# timing puzzle: taking the briefcase only sets a flag, with a 1-turn-
# delayed event actually moving it into inventory and a 2-turn-delayed
# event locking the study door (which blocks the win task once fired) --
# `open case` must land in the exact 2-filler-turn window between the two,
# too early fails one way ("not holding that"), one turn short fails a
# different way (door-lock refusal). 19 commands, no env vars.
briefcase_solution.txt|briefcase.taf|[The end]|
# The_Seance.taf (4.00): WON 100/100, the true maximum -- the game's own
# declared max is a stale 0 (mid-run `score` reports "...out of a maximum
# of 0"), confirmed against the actual sum of six ACT type=4 awards in the
# task dump. A hard real-time trap requires `open door` as literally the
# first command (dawdling 3 turns causes an NPC to leave, a permanent
# no-score ending). The game's own SYNONYM table maps bare `n` to the
# yes/no verb before movement, so full direction words are required. The
# reward locket materializes into inventory via a timed event several
# turns after a `chant` task, not at game start. Two alternate win endings
# (`yes`/`no` to join a ghost) both fire ACT type=6; `yes` scores higher.
# 18 commands, `SCR_SKIP_WAITKEY=1` (title-screen and letter waitkeys
# otherwise eat scripted input).
theseance_solution.txt|The_Seance.taf|Towards eternity with your love...|SCR_SKIP_WAITKEY=1
# reactor_1.taf (ESS Chance: Reactor 1, Justahack, 4.00): WON, one of two
# endings (a heroic-sacrifice death ending also exists via the pulse
# rifle if all three computer-repair attempts lock out) -- no scoring
# system (MaxScore=0, zero ACT type=4). Each of three mutually-exclusive
# repair attempts rolls a random outcome; this seed's first attempt
# (raise shields) locks out, the second (vent coolant) succeeds. Closing
# out the Chief Engineer's radio conversation with a plain reply is
# required before `access computer` -- leaving it open makes a bare menu
# choice resolve to the conversation instead of the computer's action. 11
# commands, `SCR_SKIP_WAITKEY=1` (three opening waitkeys otherwise eat
# scripted input).
reactor1_solution.txt|reactor_1.taf|Congratulations, You saved the ship!|SCR_SKIP_WAITKEY=1
# Motion.taf (4.00): WON 100/100, the true maximum (three ACT type=4 awards
# of 25+25+50 across 68 tasks). A three-stage rocket minigame (launch, land,
# drive-to-recover) driven almost entirely by bare-Enter "wait" moves plus a
# handful of `f`(orward)/`next`/`r`/`l` commands. Win-check tasks run one
# turn behind each stage's own state-update task, so one extra confirming
# turn (any input) is needed once a threshold is first reached, and Stages
# 1-2 (not the final Stage 3) need a second "next" to advance past the
# shared "Won!" room. 137 commands (115 blank Enter presses + 10 `f` + 8
# `next` + 2 `r` + 2 `l`), `SCR_SKIP_WAITKEY=1` (Stage 3's ASCII-art
# animation waitkeys otherwise eat scripted input).
motion_solution.txt|Motion.taf|You scored 100 out of the maximum 100!|SCR_SKIP_WAITKEY=1
# tophat.taf (4.00): the game's only ending, reached in three commands --
# no scoring system (zero ACT type=4). A one-room vignette narrated from
# inside a magician's top hat; the assistant pops up (`up`) three times in a
# row, each with different flavor text, before being sent back down for
# good. Solution is simply `up`/`up`/`up`.
tophat_solution.txt|tophat.taf|But will the next show go the same way?|
# 3 minutes1.0.taf ("Three Minutes to Live" by Ren, Hourglass Competition,
# 4.00): reaches the best of four possible endings (one survival, three
# death) -- no scoring system (zero ACT type=4 across 69 tasks). Free arms
# via `pull rope real hard right` (the RNG-fixed circularsaw variable
# deterministically resolves to 1, making "right" correct); steer a rotating
# saw via `push lever`/`press button` to cut both ankle ropes; drag a
# coroner's body onto a slab and saw off its hand to open a scanner-locked
# locker; solve a combination (revealed verbatim by `x cabinet`) entirely
# via direct-placement commands (roulette ball, dice, cards), with zero
# reliance on RNG. Reconfirms the object-*seen* model: `take jack`/`take
# ace` fail until `x table` first makes them referenceable. 28 commands,
# `SCR_SKIP_WAITKEY=1` (an intro waitkey otherwise eats scripted input).
threeminutes_solution.txt|3 minutes1.0.taf|But not a hero anymore.|SCR_SKIP_WAITKEY=1
# neighbours.taf (4.00): WON 100/100 via a custom evidence variable (no
# built-in ADRIFT score/EndGame actions) -- six score-band `call police`
# tasks dispatch on the final tally. An old-bones dig task (+3) is
# permanently shadowed by an earlier wildcard `*dig*` task sharing the same
# restriction, so it never fires (an authoring bug, confirmed live) --
# skipped in the golden. All guaranteed one-time evidence sums to 94, so the
# golden repeats `x boxes` in the Cellar (an uncapped, likely-unintended +3
# each time past the first) twice more to land on exactly 100; a fifth
# evidence source (Crumm's-Garden dig, +5) is deliberately left untouched
# since taking it would overshoot 100 and soft-lock the ending (no band
# matches over 100). 64 commands, `SCR_SKIP_WAITKEY=1` (five intro waitkeys
# plus one at the ending otherwise eat scripted input).
neighbours_solution.txt|neighbours.taf|Well done indeed!|SCR_SKIP_WAITKEY=1
# The First To Arise Alone With A Pug.taf (4.00): WON 100/100, the true
# maximum. First chapter of a larger series -- unlocking and opening the
# front door (the latter requires summoning an in-fiction power, `open
# front door with danthil`) ends the chapter. 40 commands, no env vars.
firstpug_solution.txt|The First To Arise Alone With A Pug.taf|You scored 100 out of the maximum 100!|
# ForestHouse3.taf (4.00): reaches the game's only ending -- no scoring
# system at all (zero ACT type=4, no `score` command response anywhere in
# the transcript). A time-travel/coma narrative resolving a family tragedy;
# contains a childhood-death backstory revealed through dialogue -- serious/
# dark theme, no sexual content, proceeds under normal wiring per the
# thelasthour precedent, not AIF treatment. 72 commands, no env vars.
foresthouse3_solution.txt|ForestHouse3.taf|between your gorgeous wife and beautiful son, you find that you are|
# DayAtTheOffice.taf (4.00): WON, an intentional overachievement ending --
# the in-game `score` command tops out at "47/52 out of a possible of 60"
# during play, but the closing narrative separately tracks a 1-7
# "performance" scale and this playthrough reaches 8, one better than that
# scale's own maximum ("achieved a score 1 better than the maximum"). 38
# commands, `SCR_SKIP_WAITKEY=1` (otherwise eats scripted input).
dayattheoffice_solution.txt|DayAtTheOffice.taf|I'll have a tea, black with two sugars and don't stinge on the water.|SCR_SKIP_WAITKEY=1
# beer.taf (4.00): WON 50/50, the true maximum, one of (at least) two
# possible endings ("There are will be a sequel for each of the possible
# two endings..."). Digging in the outback Bush comes up empty; the win
# path is `search dirt` there instead, finding a pouch that wins the game
# outright. 80 commands, no env vars.
beer_solution.txt|beer.taf|You search the dirt and find a pouch.|
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
transcript() {  # $1=game path $2=solution path
  { cat "$2"; echo quit; echo y; } \
    | ( ulimit -t 30; env SCR_ECHO_INPUT=1 $ROW_ENV "$SCARE_BIN" "$1" 2>/dev/null ) \
    | tr -d '\r' | sed 's/[[:space:]]*$//' | cat -s
}

# Build the harness if it's missing OR older than any engine source.  The
# missing-only check once let a whole corpus run "pass" against a stale binary
# (the wield-model port, 2026-08-01) -- never again.  os_ansi.cpp is in the set
# too: it is the port that prints the transcript (prompt, echo, line wrap), so
# editing it changes every golden while matching none of the sc*.cpp globs.
SRC_DIR="${SCARE_DIR:-$(cd "$HERE/../../.." && pwd)}"
if [ ! -x "$SCARE_BIN" ] \
   || [ -n "$(find "$SRC_DIR" -maxdepth 1 \
              \( -name 'sc*.cpp' -o -name 'os_ansi.cpp' -o -name 'mapdraw.cpp' \
                 -o -name '*.h' \) \
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
