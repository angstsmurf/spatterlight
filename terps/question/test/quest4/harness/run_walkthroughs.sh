#!/bin/sh
# Play a collection of Quest games against their walkthroughs and print a
# PASS/FAIL table.  The games are copyrighted, so they are kept local-only in
# ../games (gitignored, see ../../.gitignore); the games dir defaults there but
# can be overridden.
#
#   ./run_walkthroughs.sh [games dir]
#   ./run_walkthroughs.sh --bless [games dir]     (re)record the transcripts
#   ./run_walkthroughs.sh --win-only [games dir]  win markers only, no diffing
#   ./run_walkthroughs.sh --only <pattern> [...]  play just the games whose label
#                                                 matches (case-insensitive
#                                                 substring, or a shell glob);
#                                                 combines with --bless to
#                                                 re-record a single transcript
#
# Each game is checked twice over: the win marker has to appear, *and* the whole
# transcript has to match "<title> - transcript.txt" in ../goldens byte for
# byte.  The marker alone is a weak test -- an engine change that garbles every
# room description still ends the game, so it still passes -- and these games
# between them exercise far more of the engine than the fixtures can.
#
# Both files of each pair are ours: every "<title> - command script.txt" in
# ../goldens was derived here from the game, and the transcript beside it is
# what our engine prints when it replays that script.  Nothing in ../goldens
# reproduces anyone else's walkthrough, which is what lets them be committed
# while the games themselves cannot be.
#
# Builds the runner if needed.  Uses a fixed RNG seed for reproducibility.
# Question draws its $rand(a;b)$ from erkyrath_random() (common_utils/randomness.c),
# which is xoshiro128** once seeded, so a seeded run makes the same draws here
# as in the Spatterlight build and the same draws on every platform -- which is
# what lets these transcripts be diffed at all.

set -u
here=$(cd "$(dirname "$0")" && pwd)

bless=no; winonly=no; only=
while [ $# -gt 0 ]; do
    case "$1" in
        --bless)    bless=yes; shift ;;
        --win-only) winonly=yes; shift ;;
        --only)     only=$2; shift 2 ;;
        *)          break ;;
    esac
done
only_lc=$(printf '%s' "$only" | tr '[:upper:]' '[:lower:]')

G=${1:-"$here/../games"}

RUN="$here/question_walkthrough_runner"
# Always ask make: the runner unity-includes the engine sources, so a binary that
# is newer than question_walkthrough_runner.cc can still be older than the edit being
# tested.  The Makefile depends on ../*.cc and ../*.hh, so this is a no-op when
# nothing has changed.
make -C "$here/../.." >/dev/null || exit 1

tmpdiff=${TMPDIR:-/tmp}/question_walkthrough_diff.$$
trap 'rm -f "$tmpdiff"' EXIT INT TERM

export QUESTION_SEED=1
pass=0; fail=0; matched=0

# play  <label>  <game>  <command script>  <win-marker>  [extra runner args...]
play() {
    label=$1; game=$2; wt=$3; marker=$4; shift 4
    if [ -n "$only" ]; then
        case "$(printf '%s' "$label" | tr '[:upper:]' '[:lower:]')" in
            $only_lc | *"$only_lc"*) matched=$((matched+1)) ;;
            *) return ;;
        esac
    fi
    wtfile="$here/../goldens/$wt"
    if [ ! -f "$G/$game" ] || [ ! -f "$wtfile" ]; then
        printf '%-22s SKIP (missing files)\n' "$label"; return
    fi

    golden="$here/../goldens/${wt% - command script.txt} - transcript.txt"
    [ "$winonly" = yes ] && golden=""

    got=$("$RUN" "$@" --echo --win "$marker" "$G/$game" "$wtfile" 2>/dev/null)
    won=$?

    if [ "$bless" = yes ]; then
        if [ -z "$golden" ]; then
            printf '%-22s skipped (--win-only)\n' "$label"
        elif [ "$won" -ne 0 ]; then
            printf '%-22s NOT BLESSED (the win marker never appeared)\n' "$label"
        else
            printf '%s\n' "$got" > "$golden"
            printf '%-22s BLESSED\n' "$label"
        fi
        return
    fi

    if [ "$won" -ne 0 ]; then
        printf '%-22s FAIL (win marker)\n' "$label"; fail=$((fail+1)); return
    fi
    if [ -z "$golden" ]; then
        printf '%-22s PASS (win marker only)\n' "$label"; pass=$((pass+1)); return
    fi
    if [ ! -f "$golden" ]; then
        printf '%-22s NO TRANSCRIPT (run with --bless)\n' "$label"; fail=$((fail+1)); return
    fi
    # The transcripts run to a few hundred KB, so show only the head of a diff.
    if printf '%s\n' "$got" | diff -u "$golden" - > "$tmpdiff" 2>&1; then
        printf '%-22s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '%-22s FAIL (transcript, %s lines differ)\n' \
            "$label" "$(grep -c '^[-+][^-+]' "$tmpdiff")"
        fail=$((fail+1))
        sed -n '3,40p' "$tmpdiff" | sed 's/^/    /'
    fi
}

play Adventure           "adventure.cas"               "Adventure - command script.txt"                            "Credits room"
play Assassination       "attempted_assassination.asl" "Attempted Assassination - command script.txt"              "YOU WIN"
play BrokenMirror        "BMTSFD.asl"                  "Broken Mirror - The Screaming Fountain - command script.txt" "completed the rather short demo"
play FadeToWhite         "White.asl"                   "Fade to White - command script.txt"                        "END OF DEMO"
play Koww                "KOWW1.ASL"                   "Koww the Magician - command script.txt"                    "over the chasm"
play MagicWorld          "Magic.asl"                   "Magic World - command script.txt"                          "won the game"
play SirLoin             "sirloin.cas"                 "Sir Loin and the coming of age - command script.txt"       "To be continued"
play Space               "space.asl"                   "Space - The Final Fuck Up - command script.txt"            "pint or two"
play Uranus              "uranus.asl"                  "Uranus or Bust - command script.txt"                       "BOOOOOM"
play GatheredInDarkness  "Gatheredindarkness.cas"      "Gathered in Darkness - command script.txt"                 "Congrats"
play Mansion             "mansion.asl"                 "The Mansion - command script.txt"                          "completed the game"
play BearCampsite        "Bear Campsite.cas"           "Bear Campsite - command script.txt"                        "escape the hazardous campsite"
play EscapeHouse         "escape.asl"                  "Escape from this house - command script.txt"               "You are free"
play SirLoin2            "sirloin2.cas"                "sirloin2 - command script.txt"                             "passed the test" --tick
play SirLoin3            "sirloin3.cas"                "sirloin3 - command script.txt"                             "COMPLETED THE GAME" --tick
# Beam ends with `playerwin` followed by `do <customwin>`, and customwin holds the
# four paragraphs of ending text -- all of which Quest discards, because
# ExecuteScript returns for every line once the game has finished
# (V4Game.Part2.cs:5695-5698).  The `define text <win>` section is empty, so the
# real last thing the game prints is the landing.  See fixtures/winafter.asl.
play Beam                "Beam_1_10.cas"               "Beam - command script.txt"                                 "land on the ground with a THUD" --tick
play Darkness            "Darkness 1.asl"              "Darkness - command script.txt"                             "succesfully get out"
play HauntedHorror       "HauntedHorror/haunted_horror.asl" "Haunted Horror - command script.txt"                       "you escaped your doom" --tick
play KingsQuestV         "KQ5_Full_final_1124.asl"     "Kings Quest V - command script.txt"                        "HAPPILY EVER AFTER" --tick --seed 1
play Annabel             "annabel.cas"                 "annabel - command script.txt"                              "YOU HAVE FOUND ANNABEL" --tick
play Lovesong            "lovesong.asl"                "Lovesong - command script.txt"                             "*THE END*"
play RedSauceMonday      "redsaucemonday.asl"          "Red Sauce Monday - command script.txt"                     "You beat the game"
play ChristmasDay        "Christmas Day.asl"           "Christmas Day - command script.txt"                        "Thanks for playing"
play EscapePrison        "Escape the Prison.asl"       "Escape the Prison - command script.txt"                    "You are finally free"
play Posh                "Posh.asl"                    "Posh - command script.txt"                                 "You wake up into the first day"
play MysteryQuest        "Mystery Quest.asl"           "Mystery Quest - command script.txt"                        "job well done"
play TardisEscape        "TARDIS Escape.asl"           "TARDIS Escape - command script.txt"                        "Thank you"
play Schoolgirl          "Schoolgirl Jan Ken Pon!.asl" "Schoolgirl Jan Ken Pon - command script.txt"               "lost all her clothes"
# Incident of the Undead is only Part 1 and stops mid-sentence -- no score, no
# playerwin (see the script's header).  The marker is Buck's closing line as the
# truck reaches Wolverine, the last room, not a win.
play IncidentUndead      "Incident of the Undead Part 1.asl" "Incident of the Undead - command script.txt"               "get on my nerves"
# Theses of Canada has no ending at all -- no score, no playerwin, nothing to
# carry anywhere (see the script's header).  The marker is the Room of the
# Forgotten People, the furthest reachable state, not a win.
play ThesesOfCanada      "theses.asl"                  "Theses of Canada - command script.txt"                     "historians forgot about these folks"
play RiddleRun           "RiddleRun.asl"               "Riddle Run - command script.txt"                           "won Riddle Run"
# Forward and Back is a hot-seat dice game with no playerwin at all -- its only
# endings are "you lost!" and an all-dice-out tie (see the script's header).  The
# marker is the losing ending, which is the end of the match, not a win.
play ForwardAndBack      "Forward and Back.cas"        "Forward and Back - command script.txt"                     "you lost!"
# Blade Sentinel cannot be finished -- its gate puzzle and its Observatory are
# both walled off by authoring bugs that the real Quest engine walls off too
# (see the script's header).  The marker is the furthest reachable state, the
# Garden after the "wake" cutscene, not a win.
play BladeSentinel       "blade sentinel.asl"          "Blade Sentinel - command script.txt"                       "delightfull green surrounds you"
play AwakeningDead2      "The Awakening Dead 2.asl"    "The Awakening Dead 2 - command script.txt"                 "hand Big Joe the can"
play DefendersOfGondor   "defenders-of-gondor.asl"     "Defenders of Gondor - command script.txt"                  "rewards you for your bravery"
# Operation Rising Star is won by USE CARD READER in Room A6, which only reaches
# the reader's "action <Use>" because the game's own "verb <Use>" is dispatched
# ahead of the built-in USE (see the script's header).
play OperationRisingStar "game368.asl"                 "Operation Rising Star - command script.txt"                "reverse before passing out"
play ClayPigeon          "Clay Pigeon Shooting.asl"    "Clay Pigeon Shooting - command script.txt"                 "head back to the cabin for a scotch"
# YOU ARE A TIGER is one room with no exits, no score and no playerwin -- the
# opening scene of a game that was never finished (see the script's header).  The
# marker is the burrito's closing line, the last state change available, not a
# win.  Emptying the bottle and the burrito needs Quest's inline-brace
# arithmetic, so this doubles as the corpus test for it (fixtures/inlineexpr.asl).
play YouAreATiger        "YOU ARE A TIGER.cas"         "YOU ARE A TIGER - command script.txt"                      "You ate the whole thing"
play CellPhone           "where's my cell phone.asl"   "Wheres my cell phone - command script.txt"                 "you found your cell phone and you win"
play HospitalVisit       "Hospital visit.asl"          "Hospital Visit - command script.txt"                       "buy a new present"
play PermanantRoom       "Permanant Room.asl"          "Permanant Room - command script.txt"                       "You escape the permanant room"
play Mysts               "mysts.asl"                   "Mysts - command script.txt"                                "End of the preview"
play ExitsOfTheWorld     "Exits of The World.asl"      "Exits of The World - command script.txt"                   "next adventure"
# Escape the Elevator's win is a death: the Maintenance Room's script has Elex
# DePaw kill you and then calls playerwin, and the game defines no win text (see
# the script's header).  The marker is the killing blow.
play EscapeElevator      "EtE.asl"                     "Escape the Elevator - command script.txt"                  "fall down dead"
# World's End's two random fights are scripted turn for turn against the
# seed-1 draw stream (see the command script's header); it no longer needs
# --save-scum or --fight, so it diffs -- and joins the oracle sweep -- like
# everything else.
play WorldsEnd           "worldsend/world's end.asl"   "Worlds End - command script.txt"                           "slumps to the ground dead" --tick

play EasyMoney           "EasyMoney.asl"               "Easy Money - command script.txt"                           "YOU DID IT"
play BriceBall           "Fantasy BriceBall 001.cas"   "Fantasy BriceBall - command script.txt"                    "YOU WIN"
# FHA puts its `playerwin` *before* the msg that describes the car-keys escape,
# and its win text section is empty, so in Quest that ending is silent (same
# guard as Beam, above).  This script takes the author's other ending instead --
# the knife's two-argument `use on <Frank>`, which is the one that has the
# playerwin last.
play WhatDoYouDo         "FHA.asl"                     "What do you do - command script.txt"                       "You have survived"
play Teacher             "Teacher.asl"                 "Day In the Life Teacher - command script.txt"              "average teacher's day"
play KurriMing           "Kurri Ming.asl"              "Kurri Ming - command script.txt"                           "You Win"
# The Hobbit: Vol I declares a win text but never calls playerwin -- Vol II was
# never written and the last room just trails off (see the script's header).
# The marker is Gandalf greeting Bilbo after Gollum, the furthest reachable
# state, not a win.
play Hobbit              "The Hobbit.asl"              "The Hobbit Vol I - command script.txt"                     "you did well. Now we must be on"
play LondePerplex        "Londe Perplex.asl"           "Londe Perplex - command script.txt"                        "YOU WIN"
# "Skate ur @SS off" is a trick simulator with no playerwin, no playerlose and no
# score threshold anywhere in the file -- a demo (see the script's header).  The
# marker is the 720 spin, the biggest trick it can score, not a win.
play SkateUrAssOff       "skateurbleepoff.asl"         "Skate ur SS off - command script.txt"                      "720 spin!!!" --tick
# The two timers (300s and 60s) are load-bearing, hence --tick; the marker is from
# `define text <win>`, which both of the game's endings print.
play MetroidLite         "metroidlite.asl"             "Metroid Lite - command script.txt"                         "The remains of Ridley have been disposed of" --tick
# No playerwin anywhere in the file -- the marker is the sign-off the last room
# prints.  See the script's header for the text-to-speech `speak` statements that
# make up half the game.
play ThunderClan1        "ThunderClan mystery 1.asl"   "ThunderClan Mystery 1 - command script.txt"                "This was ThunderClan Mystery 1, Unseen Enemy, Part 1."
# Unwinnable -- no playerwin in the file and the last room has no exits.  The marker
# is the take message of the Blue Flamed Torch, the last thing the game gives you.
play DreamWeaver         "AHHHHH.asl"                  "The Dream Weaver - command script.txt"                     "you could almost swear you say a skull in the flame"
# A school project about Dachau.  Won by telling the green figure in Barracks 4 where
# you are; the marker is from `define text <win>`.  See the script header for the
# trailing-space verb rule that gates the locked door.
play SocialStudies       "final of social studies.cas" "Holocaust WW2 History 2011 Project - command script.txt"   "out tumbles the camera letter and other things you picked up"
# Timed from the first turn to the last -- eighteen timers, and the 120-turn raft
# crossing has to be walked one turn at a time, so --tick is load-bearing.  See the
# script header for the shark cycle's 21-turn period.
play OnTheFarBlue        "On the far blue.cas"         "On the far blue - command script.txt"                      "You survived! you reached a village!" --tick
# "pure chaos" wins by "goto <bed>" followed by "goto <bedtied>" -- the first names
# an object, not a room, and only counts because Quest ignores it (see the script's
# header and fixtures/gotonoroom.asl).  The marker is the bedtied room's script.
play PureChaos           "game.asl"                    "pure chaos - command script.txt"                           "You completed the demo."
# Tai & David's Amazing Maze has no playerwin -- Version 1 ends by walking into a room
# whose indescription says so -- and the gate that leads there is opened only by
# "give the gatekeeper the document of approval" (fixtures/givethe.asl).
play AmazingMaze         "TDAMAZINGMAZE.cas"           "Tai and Davids Amazing Maze - command script.txt"          "you have made it to the end of Version 1"
# "Venus flytrap: Romantic Music" is one of the three files Quest skips CheckSections
# for by name: it closes a procedure twice (fixtures/strayend.asl).  It is also a
# thorough Quest 2.x game, picking everything up with "give <X>" plus "hideobject
# <X@#quest.currentroom#>" and staging both characters with showchar (fixtures/atroom.asl).
play RomanticMusic       "musicvf1.cas"                "Venus flytrap Romantic Music - command script.txt"         "End of episode one"
# "All the news that's fit to print" has no playerwin: the paragraph-editing endgame the
# editor's desk describes was never written and "the end" is an empty room, so the marker
# is the default description printed on walking into it.
play AllTheNews          "All the news that's fit to print.asl" "All the news thats fit to print - command script.txt"      "You are in the end"
# "The Detective" has no playerwin either -- only twelve playerlose "unfortunate endings".
# The script plays part 1, the password-gated part 2 and all three special-feature
# minigames; the marker is the line part 2 signs off with.
play Detective           "The Detective.cas"           "The Detective - command script.txt"                        "You have just completed"
# The Lazy Gun Cult ends by moving to a room called "The Message" -- no playerwin, and its
# two deaths are messages rather than playerlose, so the game runs on after them.
play LazyGunCult         "lazy gun cult part one.asl"  "The Lazy Gun Cult - command script.txt"                    "You have completed part one"
# The Pilgrims Progress is the game that needed the "visited" room property
# (fixtures/visited.asl): Evangelist only turns up in the field maze once all nine
# fields carry it.  No playerwin -- the marker is the wicket gate's look text -- and
# --tick is required, since getting out of the Slough of Despond means waiting for
# the "sinking in mire" timer to fire three times.
play PilgrimsProgress    "The Pilgrims Progress (TA).asl" "The Pilgrims Progress - command script.txt"                "YOU HAVE COMPLETED STAGE 1 OF PART 1" --tick
# The Birthday Assignment is the game that needed case-insensitive block names
# (fixtures/namecase.asl): it ends in `define room <EDock>` reached by `goto <Edock>`,
# and until the name key was folded the room arrived without its script, so the demo
# stopped one message short of playerwin.  The losing endings say "Thank you for playing
# the Birthday Assignment", so the marker is the demo-only wording.
play BirthdayAssignment  "Birthday Assignment.asl"     "Birthday Assignment - command script.txt"                  "Thank you for playing the demo"

# Green Light is a QDK 3.1 tower crawl whose starting crowbar lives in a room called
# `inventory`.  The win is the giant Mervil's monologue in the library, reached only after
# all three slot cards; the vase pays a random number of coins, so the transcript varies
# run to run and only the marker is stable.
play GreenLight          "green_light.asl"             "Green Light - command script.txt"                          "your destiny will be unfolded"

# Rainbow room's exit door is a seven-switch colour code -- red, orange, blue, i.e. ROB,
# the name of the man the apartment's owner is obsessed with.  All four endings call
# playerwin; the marker is the best of them, which needs both the diary and the 911 call.
play RainbowRoom         "rainbow room.asl"            "Rainbow room - command script.txt"                         "saved a promising athlete's life"

# "treasure hunt.asl" is really a game called Spectrum: a forty-room colour maze with eight
# treasures to pile up in the safe room, and then a second half nothing advertises, in which
# a cat opens a hidden door and a flute kills the monster guarding the way out.  The marker is
# the End of Game room's text, reached only with the Prism in hand.
play TreasureHunt        "treasure hunt.asl"           "Treasure Hunt - command script.txt"                        "You have beaten spectrum"

# "The Maze" is a 15x20 hedge maze built inside a single room: four strings of y/n hold the
# walls, %xpos%/%ypos% hold the player, and every move destroys and re-creates the four
# compass exits to match the new square.  Seven riddle gates open in rainbow order, each one
# reaching the ball or dispenser the next clue needs; the two keys go in the two plinths and
# the marker is the win message from the 'x' square at the top of the map.
play TheMaze             "The Maze.cas"                "The Maze - command script.txt"                             "You walk through the gates to freedom"

# "One Robot" is a 166-room robot revolution across three continents, wired together by a
# chain of `use <weapon> on <thing>` gates and an 8x8 grid of rooms named <0,0>..<6,7> that
# stands in for flying.  Its two observatory bosses test `if exists <switch N>` the wrong way
# round, so they can only be attacked while their switches are still standing -- blowing the
# switches up, as the game tells you to, locks the run.
play OneRobot            one_robot.asl                 "One Robot - command script.txt"                            "will be banished to a faraway land"

# "ChristmaKwanzakkah" is a 2007 speed-comp game whose whole combat system is a `weapon=N`
# property compared against a threshold inside a `use anything` script, so it needs
# quest.use.object.name -- the variable Quest fills in for a catch-all use -- to be spelt the
# way ExecUse spells it.  Four shopping crowds, three bosses, three block-shaped keys.
play ChristmaKwanzakkah  ChristmaKwanzakkah.asl        "ChristmaKwanzakkah - command script.txt"                   "You got the tree and won"

# "Pyramid Of Terror" is a hunt for the letters of MARZIPAN, the one thing Sutekh can't
# abide; you win by holding the yellowy blob and THROWing it in his chamber -- dropping it
# only works in Question, because Quest's DROP honours nothing but a raw `drop` tag and the
# blob has only an `action <drop>` (oracle finding 72, confirmed in the real 4.1.5 runner:
# it answers "You drop it." and leaves the trapdoor locked).  It turned up
# three engine divergences at once: a bogus `verb <drop #@object#>` that must evaluate to the
# unmatchable name "drop ", a dangling `then` in USE CANE, and a goth revealed inside a
# never-opened sarcophagus, who only stays in scope if the container sweep is per-container.
play PyramidOfTerror     "Pyramid Of Terror.asl"       "Pyramid Of Terror - command script.txt"                    "you slew Sutekh"

# "ESPER: The Secret of Drom Bennacht" is a Typelib murder mystery whose four gates are all
# `create exit` commands -- the doorbell, TAKE WALDEN, and the word ZACAR twice -- and whose
# endgame is a four-selection interrogation where every wrong choice shoots you.  The answers
# are 3/2/3/1; the selections test no flags, so the evidence trail is optional colour.
play DromBennacht        drom_bennacht.asl             "ESPER Drom Bennacht - command script.txt"                  "do I have a story to tell you"

# "Freshman Fantastic Boy" is an adult game with no `playerwin` at all: the win is an `afterturn`
# testing ten flags (two of them misspelled, `smithsatified` and `elindasatified`) plus an exact
# score of 10.  Seven of the ten conversations offer a branch that sets the flag without scoring
# the point, which makes the game silently unwinnable, so every selection below is load-bearing.
play FreshmanFantastic   "Freshman Fantastic Boy.cas"  "Freshman Fantastic Boy - command script.txt"               "You slept with all the women in the school"

# "Enterprising in space" holds all of its state in hidden/shown object variants -- Youhera alone is
# eleven objects -- and three of the state transitions hang off `gain` scripts on bare `take` tags
# (the sandwich, the chocolate and the rope).  That is the divergence the `gainscript` fixture pins:
# before the fix those gain scripts never ran and the game could not be finished.
play Enterprising        "Enter.asl"                   "Enterprising in space - command script.txt"                "Congradulations you have made love"

# "A certain Oscar" is the opening scene of an abandoned game: two rooms joined only by getting in and
# out of a bed, no exits, no `playerwin`, and a hundred-point score nothing ever increments.  This is a
# furthest-reachable-state run; the marker is the game's one and only take script.
play CertainOscar        "Oscar.asl"                   "A certain Oscar - command script.txt"                      "You take the remote control in your hand"

# "Burglary!" is a crafting chain (towel + trowel -> break the one window with a carpeted floor; gum +
# plunger flange + laser -> cut a disc out of the wall safe) guarded by seven instant `playerlose`
# traps, including SPEAK TO SMALL KEY twice -- the game's only numeric variable exists just for that.
# Note the `2` answers: Question renders `ask` as a 1)Yes 2)No menu read with atoi, so a spelled-out "no"
# would score 0, clamp to option 1 and take the Yes branch.
play Burglary            "Burglary!.asl"               "Burglary! - command script.txt"                            "You got it"

# "Into The Briny Blue" is a straight line with one puzzle (LOWER LADDER, the game's only
# `create exit`) and eight `playerlose` traps, four of them on the same control panel.  The
# interesting part for us is that it is a `stdverbs.lib` game: eleven custom verbs implemented as
# per-object `properties <verb = text>` with game-level `verb <x> msg <>` fallbacks, so this run
# exercises the property-verb path over about sixty objects.
play BrinyBlue           "briny blue.asl"              "Into The Briny Blue - command script.txt"                  "your mission has been called off"

# "Black Forest" is Part I of a two-parter, and the interesting thing about it for the test suite is
# how much of it runs through `choose` selections and `ask` prompts nested inside them -- the manhole
# has a four-choice selection that reappears on every LOOK, and the sewer grate's fourth choice is an
# `if got ... then if ask ... then ... else ...` whose else binds to the inner if.  Also exercises
# revealobject/concealobject and a `lose` that drops an already-hidden object.
play BlackForest         "Blackforest.asl"             "Black Forest - command script.txt"                         "Part I of Devon Oratz"

# "Prison Break" has no compass exits at all -- travel is three flag-gated `go to <room>` commands --
# and its whole endgame is a ten-node tree of nested `choose` selections that `goto` one of two ending
# rooms.  Neither ending is a `playerwin`; both are a bare `stop`, so this is also our only coverage
# of `stop` as a terminator.  Ten game-level `verb ... msg` fallbacks answered by per-object
# `properties <verb = text>` lists round it out.
play PrisonBreak         "Prison Break.asl"            "Prison Break - command script.txt"                         "you have finally found somewhere you belong"

# "Mitchell Quest" replaces twenty of the parser's `error <...>` strings in one `define game` block --
# more of the default error surface than any other game here -- so every unrecognised or
# default-handled command has to route through a game-supplied message.  It also leans on `create exit`
# from six different `use <x>` handlers, a torch-gated conditional `south` exit, a four-room
# show/hide/reveal relay (the running fly gag), and a three-way disambiguation menu between objects
# with *identical* names, which the script answers explicitly.
play MitchellQuest       "MitchellQuest.cas"           "Mitchell Quest - command script.txt"                       "You wonder if Burger King is still open"

# "Michael's Game" cannot be finished: the Library declares its final exit as
# `west locked <a> locked <b> locked <c> msg <You've beaten the game>`, which parses as a locked
# *script* exit whose lock message is <a> and whose one-line script begins with the non-statement
# `locked` -- so the win text is dead code in real Quest 4 too (see the script's header, and
# fixtures/exitscript410.asl).  Every puzzle is still solved: the marker is the guard falling asleep
# and unlocking that door.  Worth keeping for the five-step `combine` chain, the broken `and`/`or`
# precedence that makes the robot's name decide whether you can progress, and heavy stdverbs.lib
# `verb ... msg` / `action <verb>` traffic across fourteen custom verbs.
play MichaelsGame        "Michaels Game.asl"           "Michaels Game - command script.txt"                        "DOOR TO THE END OF THIS DUMB GAME unguarded"

# "Blight of Elantria" is the suite's largest map (80 rooms) and the only game whose critical path
# depends on *which* room's `afterturn` runs at the end of a turn that moved the player.  The Ice
# Queen's throne room ends every turn with `if not flag <icequeendead> then playerlose`, and she
# stands in that room, so Quest's turn-start-room semantics (V4Game.Part2.cs:4116 reads the room
# index once, then reuses it at 4567-4581) leave exactly one turn to `use warhammer on ice queen`.
# Fire the new room's afterturn on arrival instead and the game is unwinnable -- see
# fixtures/afterturnroom.asl.  Seed-locked: the scroll of translation's number is `$rand(1; 10000)$`
# from `startscript`, which is 6808 under QUESTION_SEED=1 (the mirror in the tower of light reports it).
# Deliberately no --tick: the belfry bell's deafness runs on a 60-second real-time timer, and the
# echo chamber it protects is ~200 turns away, so the author's own puzzle needs the clock stopped.
play BlightOfElantria    "Blight of Elantria.cas"      "Blight of Elantria - command script.txt"                   "Elantria is in your debt forever"

# "The Legend of Cyrn" is a 2002 QDK 3.03 demo with no `playerwin` anywhere (`define text <win>` is
# an empty block), so the marker is the author's own end-of-demo notice in Path9's room script.  Kept
# as one of the few asl-version <300> games here: `define selection`/`choose`, `ask`, `enter`, `place`,
# `doaction`, and object disambiguation resolved purely by `detail` (two identically-listed Scholars,
# only the Awake one takes the scroll).  Also a nice `create exit` dependency -- the courtyard's north
# exit is gated on *not* having the gold medallion that opened the gates, so the Magic Chamber's
# `cast shield spell` (which confiscates all three medallions and creates the bypass exits) is the
# only way into the second half of the demo, and the two training rooms must be visited in order.
play LegendOfCyrn        "Magic-2.asl"                 "The Legend of Cyrn - command script.txt"                   "END OF DEMO"

# "The Statue of Riddles" is here because it used to be a black screen.  Its last room is described
# as "Yep, 100% empty." and Question threw out of QuestionFile::static_eval on the unpaired percent sign;
# the throw was caught in set_game, which abandoned the rest of startup, so the game printed no
# title, no intro, no room and answered no commands.  Quest logs "Line parameter <...> has missing %"
# as a WarningError the player never sees and substitutes "<ERROR>" for the parameter
# (ConvertParameter, V4Game.cs:6697-6701) -- see fixtures/unmatchedpercent.asl.  Beyond that it is a
# clean exercise of `choose` against twelve-way selections (eleven of every twelve answers are
# instant death), of `parent` scoping (the Skeleton of Thanatos is only reachable once the
# sarcophagus is opened), of `use X on Y`, and of chained selections -- Thanatos asks three riddles
# inside a single turn, so one "speak to thanatos" consumes three answer lines.  Deliberately routed
# the long way: riddle 6's third choice runs `playerwin` where the author meant `playerlose`, an
# instant win a third of the way in, and the marker is the closing credit line so the regression
# still covers all nine riddles.
play StatueOfRiddles     "The Statue of Riddles.asl"   "The Statue of Riddles - command script.txt"                "One Hobbit was forced into slave labour"

# "Shiversword 2" is one of the two games in the suite that cannot be finished without --tick (the
# other is its prequel, "Shiversword Tales", below).  Its `photo`
# timer (interval 10) is the sole writer of `edison = 7`, and that is the only state in which Edison
# hands over his camera -- no camera, no photograph of Simon, no fifth song, and `comppart1` refuses
# to start the talent contest while `songsknown <= 4`.  Everything after the contest (contract,
# clippers, fleece, dress, disguise, seance, Royal Court) hangs off that one interval firing, so a
# regression in tick_timers() shows up here as a hard stall rather than a cosmetic diff.  It also
# covers `interval <7>` deadlines in both directions: the `glove` timer takes the gloves off you
# again, and `elliottq` gives you seven turns to answer Elliott's question or the seance is lost.
# Otherwise it is a broad ASL 4 exercise -- a World Map hub assembled entirely from `create exit`,
# `choose` inside a procedure `do`ne from another `choose` (one "speak to sharon" consumes an `ask`
# plus three `choose <perform>` answers), `enter` free text at the Oracle, and the
# worn-clothing-as-separate-object idiom with room `script` blocks that swap the worn form back on
# exit.  The contest answers 5/3/2 are forced: Bing scores 24, and 10+8+7 is the only way past him.
play Shiversword2        "Shiversword2.cas"            "Shiversword 2 - command script.txt"                        "And the rest.... is History" --tick

# "Shiversword Tales" is the prequel, and it needs --tick for the opposite reason to its sequel: not
# one long delayed grant but a chain of seven five-turn timers used as a scene clock.  Sharon's party
# runs on timers 5/10/15/20/25/30/35, each hiding one set of guests, showing another, re-`goto`ing you
# into the room and arming the next.  Timer 35 is the sole writer of `scone = 7`, `lemon = 7`,
# `laura = 7` and `mama = 9`, and the only thing that moves John Lemon into Lutes for Loot, so the
# lute, the tuxedo, forgiving Mama, the birdcage and the ending all hang off it; without --tick the
# script stalls inside Scone Cottage, whose `south` is guarded on `flag <partyon>`.  Because the
# script is written against the exact five-turn phases (six drinks served in the four phases Mama is
# absent from, and `drinky >= 5` is what buys the tuxedo), a timer firing one turn early or late
# costs a gold star -- which makes this the suite's regression on tick cadence rather than merely on
# timers existing.  There is no `playerwin`: `theend` just `stop`s, so the marker is the closing
# prose, and the run is also checked by eye for three GOLD STARs (`mama = 11`, `cleaning = 10`,
# `got <Tuxedo (worn)>`).  Along the way it exercises `place` exits ("go to shiversword's house"),
# an object whose name really contains parentheses, a bare `flag on <>` with an empty flag name, and
# the `show`-is-not-`give` idiom about a dozen times over.
play ShiverswordTales    "shiversword1.cas"            "Shiversword Tales - command script.txt"                    "TO BE CONTINUED" --tick

# "Sutekh Is Hiding In Your Priory" is the corpus's global-command game: the priory is mostly a stage
# set for four parameterised commands -- `dial #number#`, `say #text#`, `summon #spirit#` and
# `shout #loudly#` -- each dispatching on `select case <#param#>` with multi-value cases
# (`case <satan; the devil; devil; lucifer; set; sados; saddos>`) and a `case else`, and with the
# parameters treated as free text ("dial home" is a real telephone number here).  The whole run is
# played against a budget the engine enforces itself: `afterturn` walks a nine-deep nested if/else
# over a turn counter and `playerlose`s at exactly 200 turns, while `pick apple` and `eat pastries`
# hand turns *back* via `dec <number of turns; 6>` and `dec <number of turns; 2>`.  The ghostly-priest
# interludes at 44/88/99/133/144 turns are the visible proof the chain is walked to the right depth;
# at 158 turns this script sees those five and not the sixth at 166.  Also covered: `if ask <...>`
# with a load-bearing answer (`fly` -- Yes is a death, No teleports you to the tea-room), five
# `choose` menus, `lock`/`unlock` in the `<room; direction>` form next to the author's stray
# `unlock <; >` with both arguments empty, and seven empty-argument commands (`show <>`, `flag on <>`)
# on the winning path, none of which may abort a turn.  The win is threaded past eighteen `playerlose`
# traps; the gate everybody misses is that the priesthole's Sutekh is `hidden` and only
# `summon sutekh` reveals him.  One authoring bug is reproduced deliberately: `make = cook` in
# `define synonyms` rewrites the author's own `make it stop` command to "cook it stop" before command
# matching, so it can never fire -- Quest does the same (ConvertCommand, V4Game.Part2.cs:4143-4168).
play Sutekh              "Sutekh Is Hiding In Your Priory.asl" "Sutekh Is Hiding In Your Priory - command script.txt"      "Well done, I hope you enjoyed your trip"

# Welcome to Ponyville is a stat-builder, not a puzzle game: the content ends
# when the NewDay variable reaches 14, and NewDay only moves in the ten places
# the author attached `do <New Day>` to, so the script is a thirteen-day
# schedule.  The chapter-2 ending is a bare `stop` with a chapter-end card and a
# `playerwin` written behind it (Ponyville.asl:1752-1759) -- dead code in Quest,
# hence a text marker rather than a win.  Also covers `create room <X>` for rooms
# with no `define room`, and the existence-only `property <obj; name>` test.
play Ponyville           "Ponyville.asl"               "Welcome to Ponyville - command script.txt"                 "You pack up and head out with her"

# Zombies Attack is a chance game: a dozen `define selection` blocks roll
# `inc <chance factor; $rand(1; 10)$>` after resetting the counter with
# `set <chance factor; 0>` -- lower case against the "Chance factor" its
# startscript created.  Quest resolves an untyped `set` case-insensitively
# (SetUnknownVariableType, V4Game.Part2.cs:592-618); Question did not, so the reset
# was lost, the counter only grew and no roll in the game could be passed.  See
# fixtures/setcase.asl.  --seed 4 is the stream that wins the four rolls this
# route depends on; --tick is needed both for the opening zombie and for the
# Street -> Plane -> Plane crash -> Scream -> End timer relay that ends the game.
play ZombiesAttack       "Zombies Attack.cas"          "Zombies Attack - command script.txt"                       "You completed the game in" --tick --seed 4

# Get out of the house! is a small ASL 350 house-escape with no timers and no
# randomness, kept for its mechanics: `create exit north <Kitchen; Bedroom2>`
# overwriting an exit that already carries a *script* (at ASL < 410 that assigns
# straight into the room's north slot as plain text, ExecuteCreateExit,
# V4Game.cs:5433-5437) -- without which the game is unwinnable, because the
# author's own condition tests `got <string fragment>` and no such object exists.
# Also four-deep show/hide swap chains, `exists <obj>` as the only game state,
# `gain` scripts on take, and two `script`/`beforeturn`/`script` bare keywords
# that must parse without error.
play GetOutOfTheHouse    "gooth913.asl"                "Get out of the house - command script.txt"                 "CONGRATULATIONS"

# The Former is the largest fully deterministic ASL 350 game in the corpus: 74
# rooms, no timers, no `rand`, and a two-act detective plot whose act break is a
# ten-term `if exists <...> and not exists <...>` over invisible marker objects
# (room `end`, object `PDA2`) -- seven that must have been `show`n and three that
# must have been `hide`den, with the author's own inconsistent spacing inside the
# object names (`clue-test9`, `clue- test 10`, `clue -test 4`).  It also repeats
# the ASL < 410 `create exit` behaviour that "Get out of the house!" found, but
# here it works against the player: `use <Flashlight>` in mine4 runs `create exit
# east <mine4; mine5>`, which overwrites the scripted exit whose only job was to
# `show <test - clue 6>` when you walk into the dark (ExecuteCreateExit,
# V4Game.cs:5433-5437).  Buy the torch before ever bumping into the darkness and
# the game silently becomes unwinnable while still reporting "Clues: 15/15" --
# so this script walks into mine4 blind first, on purpose.
play TheFormer           "former.asl"                  "The Former - command script.txt"                           "have learned the whole truth"

# The Devil's Bargain predates the standard library and rolls its own, which is
# where the corpus's only Quest 3/4 room-as-container games live: a container is a
# *room* the player is silently walked into (outputoff / goto <desk> / read
# #quest.formatobjects# / goto back), so `define room <desk>` coexists with the
# `define object <desk>` the player sees -- five such pairs here (box, desk,
# wallet, briefcase, panel 579).  Quest keeps rooms in _rooms and objects in
# _objs, so it never confuses the two; Question resolved the name to the room, which
# printed the room's look text for `x desk`, pasted the room's listing prefix onto
# the object, and made `here <desk>` false so the library refused to hand anything
# over.  An unqualified lookup now answers for the object and the room half is
# named explicitly, including through the deprecated room-scoped `gettag`
# (V4Game.cs:6899), which Question did not implement and which this library needs both
# for a container's prefix and for `$gettag(X;look)$` returning the literal
# "object"/"character" to tell the two kinds apart.  It also found `nointro`
# (the game displays <intro> itself, after its three setup questions) and `do`
# followed by an inline brace block, which the loader rewrites into `... do do
# <!intprocN>`; see fixtures/roomobjname.asl, nointro.asl and doblock.asl.
# Five different moves call `playerwin`; this route buys the guitar the game's
# own objective text names, funded by selling the church computer's monkey
# drawing to the bank ($850) and the captain's photos back to the crooked officer
# who is in them ($800).
play DevilsBargain       "Bargain.cas"                 "The Devil's Bargain - command script.txt"                  "You have won The Devil's Bargain"

# Mario Is Missing 2 is the corpus's timer game: it cannot be finished without
# `--tick`, because the only route to Fahr Outpost (and thus to either ending) is
# a 60-tick timer armed by talking to Bootler, hence the 60 no-op `look`s near the
# end of the script.  Two shorter timers are lethal instead of enabling -- picking
# up a cannon arms a 5-tick death, and smashing the nuclear reactor a 15-tick one
# that has to be outrun to the bomb shelter in Toad Town.  It also pins the
# version split in Quest's bare `use <item>`: below ASL 410 the current room's own
# `use <item>` list is consulted before the item's own use action, and from 410 on
# it never is (ExecUse, V4Game.Part2.cs:5348-5368); Question checked the room first at
# every version.  This game is 350 and needs the room to win twice, since the
# vacuum cleaner's own use only comments on the noise while the two rooms that
# matter use it to uncover the Bob-omb doll and the door to Lady Bow -- see
# fixtures/bareuseroom.asl and bareuseroom410.asl.  Both endings call `playerwin`;
# this route takes the one the author labelled "REAL" (and then announced as the
# secret one, which is where the win marker comes from).
play MarioIsMissing2     "MIM2UQ.asl"                  "Mario Is Missing 2 Ultimate Quest - command script.txt"    "Congratulations on finding the secret ending!" --tick

# Shipwrecked is the corpus's big QDK treasure hunt: three islands, 419 turns,
# four scripted fights won with fixed menu answers under QUESTION_SEED=1, and a 3x3
# sliding-tile door.  It is also the game that exposed four container/parsing
# divergences, each pinned by a fixture: `list empty` and `list closed` were
# being dropped by the loader so the raft could never be finished
# (fixtures/listempty.asl); `repeat until <cond> <script>` without the `do`
# keyword was discarded wholesale, which silently disabled the colored-hole
# puzzle (fixtures/repeatcond.asl); a scripted `out { ... }` exit pasted its
# de-inlined script text into the room description (fixtures/outscript.asl); and
# `put X in Y` never consulted the target's `add` action or property -- which is
# Quest's own container mechanic, DoAddRemove, V4Game.cs:2578-2636 -- so neither
# `put lantern in basket` nor `put box of matches in jar of honey` set the flags
# the tomb and the swim depend on (fixtures/containeradd.asl).  Finally `got <X>`
# looked only at the immediate parent, so the matches stopped counting as carried
# once they were sealed in the honey; Quest copies the container's ContainerRoom
# onto whatever is added to it (V4Game.cs:2117-2121), so Question now walks the chain
# the way `here` already did (fixtures/gotcontainer.asl).  Note the deliberate
# `look at <container>` before every `open <container>` in the script: the game
# defines its own `verb <open>`, which bypasses the built-in open command and so
# never marks the container "seen" -- and an unseen container keeps its contents
# out of scope.  That is faithful, not a bug.  All three endings call `playerwin`;
# this route takes ending1 at 30/400, since the richer endings want every treasure
# banked in the sail boat's glass case.
play Shipwrecked         "Shipwrecked1.3.cas"          "Shipwrecked - command script.txt"                          "You have completed Shipwrecked"

# Barbarian is the corpus's longest fight: 639 script lines, 512 turns, 250/250,
# and eight scripted battles plus a five-stage trap all won with fixed menu
# answers under QUESTION_SEED=1.  Every optional fight is compulsory, because a win
# is +1 maxhealth and Gak only teaches the Vault above 16 -- and the Vault is the
# one thing in `thorgincombat` that clears `enemymounted`, so without it the
# final duel never leaves its mounted loop.  Three divergences came out of it,
# all fixtured.  The sack of bones must be OPENed before the bones go in and
# CLOSEd again for the pedestal: Quest refuses PUT into a closed container
# before its add machinery is consulted (ExecAddRemove, V4Game.cs:2563-2578;
# fixtures/putclosed.asl).  The game's own `unwield` command computes
# `$left(#command1#; %length1%)$` over "Battle axe (wielded)" to strip the
# " (wielded)" suffix; Question ended the argument list at the *first* right paren,
# which cut it down to one argument, so the function returned nothing and the put
# away weapon was never handed back -- Quest's DoFunction takes the last paren
# (InStrRev, V4Game.cs:6745), fixtures/funcparen.asl.  And Quest records
# containment as the child's own "parent" property, which games read back as
# `#child:parent#` (DoAddRemove, V4Game.cs:2113-2133); Question kept containment only
# in ObjectRecord::parent, so the crypt pedestal (which trades the Eye of the
# East for a sack only while `is <#Pile of bones:parent#; Sack>`, and springs a
# lethal trap otherwise) and the hedge-maze well winch, which re-parents the
# handle at each of four assembly stages, could never match -- fixtures/
# containerparent.asl.  Note that TAKE deliberately does *not* clear the
# property: ExecTake reads the parent and never sets its own isInContainer flag
# (V4Game.Part2.cs:5160-5204), so a thing taken out of a chest stays "parented"
# to it for good.  That is faithful, and Barbarian depends on it.
play Barbarian           "Barbarian.cas"               "Barbarian - command script.txt"                            "You have finished Barbarian!"

# The Lazst Resort is scored out of 125 and the win text ends in a rank drawn
# from a ladder whose top rung, "Perfectionist", exists only at exactly 125 --
# so the marker here is a perfect-score assertion, not just an ending check.
# Most of the 125 is paid out for using the fading hotel's fixtures one by one
# (telephone, toilet, shower, clock radio, jukebox, piano, vending machine), so
# the route is deliberately exhaustive.  Two puzzle answers are not in the story
# file at all -- they live in the .jpg files the game shows with `picture`, and
# the ASL holds only the bare comparisons: the housekeeping padlock's 8/20/6 is
# on dartboard.jpg and the island keyboard's "cion" is in the kids' crossword on
# crossword.JPG.  Both were taken from the source's literal comparisons instead,
# since a headless runner shows no pictures.  --tick is required for
# `drinktimer`, interval 30: Jack takes thirty turns to mix the cocktail (worth a
# point) and the timer action checks `if here <jar>`, so the wait has to be spent
# standing at the bar; the script reads the newspaper cover to cover there, which
# is also where the crossword is.  No engine change came out of this game, but it
# pins one piece of faithfulness worth recording: the author's own
# `command <drive>` and `command <type on keyboard>` can never fire, because
# `define synonyms` maps `drive = use` and `type = use` and Quest substitutes
# synonyms across the whole input line before command matching -- it wraps the
# input as `" " + input + " "` first, so even a bare one-word `drive` is caught
# (V4Game.Part2.cs:4149-4163).  Question does the same; the live routes are USE CAR
# and USE KEYBOARD.
play "The Lazst Resort"  "resort.asl"                  "The Lazst Resort, Part 1 - command script.txt"             "Perfectionist" --tick

# Wizard is the longest route in this suite at 1090 turns, and most of that is
# waiting: the Swirling Misty Portal that links the tower to the six outer areas
# cycles black/red/yellow/blue on a 15-turn `portal1` timer, so every trip has to
# be lined up with a block of WAITs, and the portal only announces its colour on
# room entry or on LOOK AT PORTAL.  --tick is therefore mandatory -- without it
# `portal1`, the 60-turn `door1`, the 45-turn `idiot` and the 40-turn `int2`
# never fire and the game is unfinishable.
#
# The one place the route depends on the RNG is also mandatory, which is why the
# suite's QUESTION_SEED=1 matters here.  The Enlighten spell is only unlocked by
# reversing Enfeeble, and `command <rmemorize>` refuses to reverse a spell whose
# description is still `?` (wizard.asl:274-320) -- so the player has to cast
# Enfeeble on himself, which sets `idiot2`, and `beforeturn if flag <idiot2> then
# dontprocess` then eats 45 turns of input while an `afterturn` rolls
# $rand(1; 5)$ each turn.  Roll 5 makes him throw the spellbook away, so the
# script takes it back afterwards; under a different seed that pickup lands on a
# different turn.  This is the game's only random draw.
#
# Two of the puzzles are drawn entirely with `picture` calls and so are invisible
# to a headless runner: `define procedure <table1>` (wizard.asl:5056-5085) paints
# the 4x4 bead grid as .jpg files, and the answer (c1, a2, d3, b4) was read off
# the sixteen `case <a1>`..`case <d4>` flag toggles instead.  The sixteen Yellow
# Beads are all named "Yellow Bead", so every TAKE YELLOW BEAD goes through a
# disambiguation menu ordered by object *definition* order counting inventory and
# room candidates together -- the indices in the script were computed from the
# `define object <Yellow BeadN>` line numbers and are position-sensitive.
#
# The endgame does a full recon trip before pulling the lever that sets `end1`,
# because once `end1` is on the Moat drowns the player on entry
# (wizard.asl:4647-4744) and the Stained scroll behind it can no longer be
# fetched.  No engine divergence came out of this game; it did supply two more
# instances of the faithful `seen` gate (the Label inside the Bottle of sludge
# and the Copper coin inside the Small purse are out of scope until the container
# has been LOOKED AT).
play Wizard              "Wizard1.3.cas"               "Wizard - command script.txt"                               "You have finished Wizard!!" --tick

# The Things That Go Bump In The Night is the biggest game in this suite: 87
# rooms across four buildings, 319 turns, and the only route that has to satisfy
# five independent timed set-pieces.  Almost every room is `properties <dark>`
# and several exits test `is <#Flashlight:suffix#;(providing light)>`, so the
# TURN ON FLASHLIGHT on turn 8 is what makes the other 330 lines legal.
#
# It is also the game that motivated the implied-REMOVE-before-TAKE fix.  The
# water pump will not reset until all four good fuses are parented to the Fuse
# Box, and the box's `add` script counts what is still parented to it:
#
#     for each object in game if is <#(quest.thing):parent#;Fuse Box> then inc <objectcheck>
#     if is <%objectcheck%;gt=;4> then msg <There's no room. The box is full of fuses.>
#
# Question used to take a parented object without clearing its `parent`, so pulling
# the four dead fuses out left the box permanently "full" and the game
# unfinishable.  See fixtures/takefromcontainer for the pin, and fixtures/is-
# compare for the operator-splitting bug that the same investigation turned up.
#
# --tick is mandatory: the Fuel Pumps octopus (`oct`, interval 20), both
# electronic key pads (`kp5`/`kp6`, interval 15) and the blob (`blob1`, interval
# 120) are all timers, and the Smelting Plant spider is driven from `beforeturn`
# off a 3x3 `location2` grid.  The route freezes the spider with SHINE LIGHT ON
# SPIDER (two turns) and USE RADIO (one turn, and it is also how Neva is talked
# through the overhead ladle controls) in order to walk the ladle to the centre
# square, then shoots the chain out from under it.
#
# Four things in the script look wrong and are not.  The fire extinguisher is
# dropped in East Smelter Courtyard before the octopus fight because the fight's
# menu needs it *in the room*; LOOK AT CABINET and LOOK AT GUN RACK are there
# because of Question's faithful `seen` gate (a container's contents stay out of
# scope until the container has been examined -- see fixtures/containervis);
# LOCK DOOR after the control room is required, since Refinery Main Floor's
# entry script raises the chain hoist again if `chainlift1` and `kp11` are both
# set; and the bare LOOK in the Secret Labratory is a sacrificial turn, because
# that room's `beforeturn` is `if here <Dave> then dontprocess`.
#
# No new engine divergence came out of the second half of the game.  It did
# confirm empirically that Question keeps `invisible`/`conceal`ed objects in scope
# (EXAMINE LAB BENCHES works on an object that is never `reveal`ed), which is
# what Quest does -- `hidden`/Exists is the scope flag, `invisible` only affects
# room listing -- and it is what makes the clown puzzle solvable at all.
play Things              "TheThingsThatGoBumpInTheNight.cas" "The Things That Go Bump In The Night - command script.txt" "Not on my watch." --tick

# Assassin is the widest map in the suite -- 104 rooms drawn as an actual street
# grid -- and one of the narrowest games in it: a four-mission chain that visits
# maybe fifteen of those rooms.  No timers, so no --tick.  The value of having it
# here is coverage of ASL 311 selection plumbing: fourteen `define selection`
# blocks, several of which `choose` a second selection from inside a choice
# (Paul's shop menu chains into `Accessories`, and `Drive` re-chooses itself), one
# `enter <password>` prompt, and a room script that `exec`s SPEAK TO for the
# player.  All of that is answered from the script by option number.
#
# The route deliberately ignores the game's whole money economy.  `cash` starts
# at 20 and the mission payouts (500 + 1000 + 1000, then 4000) cover the only two
# purchases the chain requires, the Uzi and the silencer, so the four
# lockpickable houses, Paul's buy-back menus, `gamble` (a bare `$rand(1;2)$`) and
# the street race are all skipped.
#
# Two gates are worth knowing about, since both are easy to walk into blind.
# `More E Elm`'s west exit refuses to open while `mission 1` is held, so the trip
# out to the Red Jacket Man is a dead-end corridor; and `W. Hilton 1`'s west exit
# sets `security; active` on the way into the garden, after which the mansion's
# upstairs hallway kills the player on entry unless FLIP SWITCH has been answered
# with `tiffany` -- the name found on Louie's killer in the duck-pond cutscene.
#
# Three of the game's own bugs shape the script rather than break it.  The vault
# levers `#lev1#`/`#lev2#`/`#lev3#` are never declared as variables and are only
# initialised by SafeSmart Bank's entry script (all "up", and only on the
# `got <mission 3>` branch); Levers 2 and 3 have no `alias`, so they answer to
# "lever 2"/"lever 3" and not to "2nd lever"; and buying a gun `clone`s it under
# the name "Buy Uzi", which means the `police` selection's `got <Uzi>` test can
# never be satisfied by a purchased weapon.  The Angry Guard in the bank survives
# only because it checks the separate invisible marker `have uzi` instead.  The
# route never provokes a cop, so the broken branch is never reached.  No engine
# divergence came out of this game.
play Assassin            "assassin.asl"                "Assassin - command script.txt"                             "the corruption was left to the Mafia"

# MagicSword Part 1 is here for one line of engine behaviour that nothing else in
# the suite covers: the pre-ASL-280 game-block `use <X>` fallback.  Dom declares
# the use scripts for the Sword, the Staff, the Immune Potion and the healing
# Potion in the *game* block rather than on the objects, and below ASL 280 Quest's
# ExecUse (V4Game.Part2.cs:5472-5497) looks in the room's use list, then FindLine
# over the game block, then gives up -- the object's own use action is never in
# the chain.  Question used to stop at the room, so USE IMMUNE POTION answered with
# the game's own defaultuse text and the potion could not be drunk; since the
# potion is the only thing that sets %Immune% and Hallucination Forest kills
# anyone who enters without it, that one gap made the game unwinnable at the
# fifteen-turn mark.  The route drinks both game-block potions, so it exercises
# the fallback twice.  fixtures/gameblockuse pins the precedence rules.
#
# Two other pre-281 behaviours it leans on are faithful and deliberately left
# alone.  Procedure 79 wants an ally for the Golomoshk fight and does
# `showchar <Azambri@GProad1>` on a character defined in another room, which
# pre-281 SetAvailability cannot resolve; the author wrote his own no-ally
# fallback into the same procedure, so the fight just runs solo (seven flam casts)
# and Azambri still appears in the endgame scene, where she is shown without a
# room qualifier.  And FI3's `north if has <Unlocked;=6>` shortcut into FDO is
# dead code: there are five hollows in the stone and five keystones in the game,
# and the fifth `use keystone on stone` calls procedure 110, which teleports the
# player into FDO immediately.  FDO has no way back out, so the script does the
# whole forest sweep -- all five keystones, plus two inn stops and a healing
# potion for Health -- before it goes near the clearing.
#
# The win is the author's bookmark rather than a climax: the Guardian of Maren
# says "There is nothing beyond the door leading outside.  It has not yet been
# created", asks whether you are finished, and a yes prints "You Win!" and calls
# the game's one `playerwin`.  The illness plot does resolve on the way, and it
# has to -- GH1's gatehouse place refuses to open while %Illness% is 1.
play MagicSword          "MagicSwordP1.cas"            "MagicSword Part 1 - command script.txt"                    "You Win!"

# Ghost Light (the file is asdarknessfalls2.cas; the game block says Ghost Light)
# is the suite's only game whose win depends on a timer firing on schedule, so it
# is the strongest test --tick has.  The single source of salt in it is boiling a
# pot of sea water dry over a campfire, which `define timer <boilaway>` does after
# `interval <20>`; with no ticks there is no salt, and with no salt the demon
# dodges the falling chandelier and kills the player.  Three more timers shape the
# route rather than merely decorate it: `pentime` (20) is how long a laser pen
# stays balanced on a tripod while the player runs seven rooms to chalk the grave
# it is pointing at, `demontime` (20) is the deadline on every turn of the
# endgame, and `pausetime` (70) is the reprieve the salt circle buys.
#
# It also covers two `enter <code>` prompts answered from the script (7856 for the
# safe, from A=7 on a matchbox, B=8 on the front door wall and "C=AxB" on a desk;
# 4190 for the manor door, from "FOR ONE I KNOW" read as four-one-nine-nought) and
# three `define selection` dials that have to add up to exactly 118.
#
# The one thing worth knowing before reading the script is that EXAMINE and LOOK
# AT are different commands here and the game leans on it.  The game block defines
# `verb <examine;x>` as a catch-all brush-off, so any object that keeps its secret
# in a `look` script -- the gravestones, the rubbish, the cooking pot -- has to be
# LOOKed AT, while objects with their own `action <examine>` answer to either.
# The altar is the one that matters: LOOK AT ALTAR just describes it, and EXAMINE
# ALTAR is what finds the chisel that frees the spear the demon is killed with.
# Question already gets this right, and no divergence came out of the game.
play GhostLight          "asdarknessfalls2.cas"        "Ghost Light - command script.txt"                          "YOU HAVE JUST KILLED THE DEMON" --tick
# Nearco (Spanish, ASL 410) cannot be finished.  The three rooms holding the three
# coins the endgame needs are each walled off by a declared `<dir> msg <...la losa
# que bloquea el paso...>`, and the three stone cylinders in the palace are meant
# to open them with `create exit north <PBE2; PBF2>` -- which from ASL 4.10 on
# reuses the direction's existing exit object and leaves its script in place, so
# the refusal still runs and the door never opens (see the script's header and
# fixtures/createexitscript410.asl).  Below 4.10 the same statement would have
# worked, which is how "Bear Campsite" gets past its grizzly; The Maze, also 410,
# needs the 4.10 rule as much as Nearco is broken by it.  The marker is the
# resurrected Persian's farewell in PBE2 -- the last thing the game has to say
# once all three cylinders have been pushed -- not a win.
play Nearco              "Nearco.cas"                  "Nearco - command script.txt"                               "actor secundario"
# A day at the redsauce office (game329.asl) is Red Sauce Monday's companion
# piece: one give-chain through nine colleagues, ending in a rubber band round a
# crocodile's snout.  It never calls playerwin, so the marker is the rubber
# band's `use on <CROCODILE>` text.
play RedSauceOffice      "game329.asl"                 "A day at the redsauce office - command script.txt"         "You beat the game"
# Kingdom (ASL 300) is a one-room management sim with no `playerwin' at all and
# eight `playerlose' endings, so this is a furthest-state pin: six seasons of
# firing knights to cover the one-gold-per-knight upkeep, until Village3 revolts
# and the army is too small to put the revolt down.  Its real value is the
# onchange ping-pong: a sacked village's population floors at 1, so V<N>max
# (= pop/2) hits 0, and V<N>army's handler then clamps to 0 and rebounds to 1
# forever, announcing the leader's son each time.  Quest runs onchange with no
# re-entrancy guard (V4Game.Part2.cs:526-530) and would blow its stack; Question
# bounds the recursion, and the ~500 repeated lines in this transcript are what
# that bound looks like.
play Kingdom             "Kingdom.asl"                 "Kingdom - command script.txt"                              "We have been defeated by Village3's army"
# Revenge of the Shadow Masters is an unfinished demo -- no `playerwin', and the
# Refuge of Riddles prints "You have reached the end of this version" while three
# further rooms still lie south of it -- so this is a furthest-state pin ending on
# the last room that has any exits at all.  Two things earn it a place here: the
# room key arrives from a `define timer <get a room>` with `interval <5>`, so it
# needs --tick, and the Mug o' Grog is `properties <hidden; Drink; buy; steal>`
# with no bare `hidden` tag.  Quest's loader undoes a load-time hidden that came
# from a properties line (V4Game.Part2.cs:3550-3553), so the grog is on sale from
# turn one; Question used to hide it, which made the Drink of Immortality unmakeable
# and the whole second half of the demo unreachable.  fixtures/hiddenprops.asl
# pins the rule itself; this is the corpus case that found it.
play ShadowMasters       "Revenge of the Shadow Masters.asl" "Revenge of the Shadow Masters - command script.txt"        "you see what you believe to be a dwarf" --tick
# The Quest to find The Dark Hills is the finished game that Shadow Masters is a
# sequel demo to, and it is a real win reached entirely through timers: there is
# no `playerwin', and the only thing that ever notices the last enemy is dead is
# `Alexendre Attack' on its own six-turn tick, which prints "You've defeated
# him!" and chains six credit timers ending in THE END and `stop' (hence
# running=false, and hence --tick).  Every fight is the same shape -- an "On"
# timer shows the enemy and sets enemy health, an "Attack" timer checks for the
# kill only when it ticks -- so the runs of `wait' in this transcript are the
# route, not padding.  The marker is "defeated him" rather than "THE END"
# because marker matching is case-insensitive and the tunnel under the cathedral
# mentions "the end of the tunnel" first.  Worth having for the timer chains,
# the `afterturn' exit gates that only open on the turn after they refuse you,
# and the three-way fork at Zephyrus Way, of which this takes the trading branch.
play DarkHills           "The Quest to find The Dark Hills.cas" "The Quest to find The Dark Hills - command script.txt"     "defeated him" --tick

# Operation: Sleepover is a transformation game (age regression, so the player
# character is a toddler for a stretch and a nine-year-old girl for the endgame)
# built on the author's own "QNA" clothing library, and that library is the
# reason it is in the corpus: garments are `type <clothes>' subtypes carrying
# pant/shir layer values and a size, `qna.wearable' refuses anything whose layer
# is already covered or whose size is outside %size%..%size%+1, and every
# transformation re-derives %size% from %age% and resizes the whole worn
# wardrobe through `for each object in <inventory>' + `$objectproperty()$'.  No
# `playerwin': all endings run `do <thend>', which walks the inventory to print
# what you were wearing and carrying and then calls `stop'.  The marker is "job
# well done" because that line of `thend' is guarded by
# `if property <atari; dead> and got <cookies>', so it pins the good ending --
# markers taken from earlier lines of `thend' would also match the two deaths by
# electrical outlet.  Caution when editing this transcript: the party door asks
# you to type your name and compares it to a name `qna.randnames' picked with
# $rand(1;20)$ at startup, so the "Hannah" line is only right at seed 1.
play Sleepover           "sleepover.cas"               "Operation Sleepover - command script.txt"                  "job well done"

# Metal Sonic's Quest is the game that found the `gametype multiplayer' bug: Question
# used to throw "Error: Question is single player only." from the middle of the loop
# that reads the game block, which also skipped the `start' line below it and
# left the player in room "" with all 148 rooms in scope.  Quest never reads the
# declaration (V4Game.Part2.cs:7992-7999 scans the block for "start " and
# nothing else), so Question now ignores it; see fixtures/multiplayer.asl.  --tick is
# mandatory and, unusually, the timers are load-bearing rather than lethal:
# `Campaign Mode- Skydive' is sealed until `O400 hours' (interval 30) shows the
# `JUMP!' object, and `Campaign Mode- Fight' until `wait for it.....' (interval
# 5) shows the `gun' -- which is why this transcript contains a run of 30 and a
# run of 5 `look' turns.  There is no `playerwin' anywhere; `define text <win>'
# is the credits roll and it is displayed by exactly one room, reached only after
# BOTH game modes are finished (main game -> `use Hidden Key' -> Extra chapter ->
# Campaign mode -> three `show'n unlockables in the "Metal's Past" rooms), so the
# marker is the credits' own boast.  Note the author misuses `place <A; B>'
# throughout as if A named a room, so every movement line is the full destination
# room name.
play MetalSonicsQuest    "MSQ.asl"                     "Metal Sonic's Quest - command script.txt"                  "you have beaten both game modes" --tick

# Something 'Bout A Hex is the largest game in the corpus (6612 lines) and the
# longest transcript in it: an autobiographical Baltimore memoir told out of
# chronological order, whose seven chapters are colour-coded years
# (1 9 r e d 9 1 ... 2 0 v i o l e t 0 4), each of which is both a `define room'
# and an `invisible' object you pick up to jump there.  It is here for three
# separate reasons.  (a) The timers: seven `timeron' chains carry the whole of
# Book Two, and because `SetTimerState' never resets TimerTicks and each step
# only turns off its PREDECESSOR, the chains cost interval + 1 turns per step and
# the earlier links keep firing alongside the later ones -- which is why this
# transcript contains runs of 14, 38, 23, 70, 43, 23 and 46 `look' turns and why
# --tick is mandatory.  (b) `use' scope: `ExecUse' takes the item from the
# inventory alone and the use-on target from the room first
# (V4Game.Part2.cs:5289, 5376-5390), so every vehicle has to be `take'n before it
# can be `use'd, and Dougherty's Pub -- where Derrick pours three separate drink
# objects that all alias "Jim-n-Ginger" and the untouched ones sit in the room --
# only works because of it.  Question used to search both scopes at once and raise a
# disambiguation menu Quest never shows, silently eating the next script line.
# (c) Trailing spaces in object names: `define object <Journal >' handed over as
# `give <Journal >', plus two bricks aliased `The Brick ' and `The Brick  '.
# Note `take iguana', not `take iquana': the author's `define object <iquana>' is
# aliased `iguana', and Quest matches aliases only (V4Game.cs:4704-4727).  Every
# `go to' line is a room ALIAS, per the ASL >= 311 `PlaceExist' rule.  No
# `playerwin' anywhere -- the marker is the congratulation `choose <octoberiffic>'
# prints in its `finalscene' branch after you press SEND on the last MSN message.
play SomethingBoutAHex   "something 'bout a hex.cas"   "Something 'Bout A Hex - command script.txt"                "completed BOOK TWO of Something 'Bout a Hex" --tick
# Kings Quest V Part One is the earlier release of the game above, and it does
# have a `playerwin': the room script of the ledge the rope leads to, which is
# where the full release carries on with `cross gap'.  The script is the first 194
# lines of the full one with two extra `look's, each for a real difference in the
# source: the rat timer is `interval <22>' here against 20, so `take rope' lands
# while Graham is still tied up; and `hungry' is only set by the mountain room's
# `afterturn', which does not run on the turn you walk in, so an immediate
# `eat lamb' is refused and he starves.  --tick is mandatory (the rat).
play KingsQuestVPartOne  "KQ5_PartOne.asl"             "Kings Quest V Part One - command script.txt"               "successfully completed Part One" --tick --seed 1
# Doctor Who's 50th Birthday Cake: a custom `command <say #speech#>' with
# `select case', some thirty game-level verbs with per-object `action' and
# `properties <verb = text>' overrides, `unlock <room; dir>', and a
# beforeturn/afterturn turn counter.  Nearly every wrong verb is a `playerlose'.
# The sacred key is taken straight out of the open savaloy, never by way of
# carrying the savaloy.
play DoctorWho50         "Doctor Who's 50th Birthday Cake.asl" "Doctor Who's 50th Birthday Cake - command script.txt" "you've completed your assignment"
# Gaiaonline Q&A is not a game but an FAQ: six rooms and 187 global
# `command <question> msg <answer>' lines, no objects, no state and no
# `playerwin', so the script is a tour ending on the author's sign-off question.
# It is here for the literal matching of patterns full of `?', `!', `,', `'',
# `/', `.' and `&', for the one question Quest cannot answer -- `command <How do
# I get a MC? >', a lone pattern whose trailing space Quest keeps and so never
# matches, and which Question trims and answers ON PURPOSE (a `deliberate:' row
# in the oracle sweep; fixtures/cmdspace.asl) -- for exits that lead back to their own room, for two exits to a room the game never defines (the
# player stays put and nothing is printed), and for a `verb' in the game block
# attached to no object, which is never matched.
play GaiaonlineQA        "QA.asl"                      "Gaiaonline Q&A - command script.txt"                       "one of the developers of this feature"
# Cabin Fever: one room, ten jigsaw pieces, and all progress modelled by
# hide/show of numbered variants of the same object (Fireplace 1-4, Puzzle 1-5).
# Line 1 answers the opening four-way menu.  Portable things stay hidden until
# their container is looked at.  The marker is the last `msg' before the only
# `playerwin'.
play CabinFever          "cabinfever.cas"              "Cabin Fever - command script.txt"                          "only to take his place yourself"
# As Darkness Falls (the game block says "Darkness Falls") precedes Ghost Light
# above and shares its EXAMINE-is-not-LOOK-AT split.  The marker is in the good
# branch of the ending only; "YOU HAVE WON" is printed by both.  Three
# `enter <var>' prompts behind `if ask', a nested `enter' menu chain on the
# office computer, four objects aliased `rope' of which only one is ever
# visible, and a chess puzzle whose flags are set as `NP' and tested as `np'.
# The `zombie' timer is dead code -- nothing turns it on -- so no --tick.
play AsDarknessFalls     "as darkness falls.cas"       "As Darkness Falls - command script.txt"                    "She kisses you and thanks you"
# City of Blood: about a hundred rooms and 498 turns.  The four `look <container>'
# lines are not padding.  The game opens its crate, trunk and safe with a scripted
# `open <obj>', which unlike the player's own OPEN does not look at the container
# (DoOpenClose with showLook false, V4Game.Part2.cs:586), so it is open but not
# "seen" and its contents stay out of reach; `take bust' answers "That doesn't
# work." in Quest too.  Also `afterturn' running `doaction', `create exit' in both
# forms, a script-valued exit, and `speak' into a `choose' whose seventh choice is
# the only `playerwin'.
play CityOfBlood         "City of Blood.cas"           "City of Blood - command script.txt"                        "buy Alkazar a drink"
# The Mansion II, the sequel to Mansion above.  Driven by `choose' menus on
# `examine': the safe is four chained ten-way menus, the locker four chained
# thirteen-way ones.  28 objects all aliased `Flask' are swapped by hide/show, so
# exactly one must be visible at a time.  The teleporter and the dream candle
# both depend on `lose' dropping the object in the current room (ASL >= 280)
# rather than removing it.  The last room is `if got <DVD-ROM2> then playerwin
# else playerlose'.
play Mansion2            "mansion2.asl"                "The Mansion II - command script.txt"                       "disk full of incriminating evidence"

# Sim Political Career: a timer-only life simulation with no rooms, objects or
# commands.  The start script takes three `enter' answers (first name, surname,
# `boy'), then five timers (intervals 1/4/12/16/20) run everything, so --tick is
# mandatory and each `look' is a filler turn advancing one quarter-year.  `choose'
# menus and `enter' prompts fired from timer scripts consume script lines mid-turn
# (menu numbers, AGM position numbers, one child name).  The only win is being
# made Prime Minister (MP with political power >= 100,000,000 under a Democrat
# government), after which the game stops its timers.  Every promotion and
# election is a `$rand(1;100)$' test, so the script is tuned to seed 1 and to the
# engine's draw and timer order, not a general strategy.  Exercises float numeric
# variables (age steps by 0.25, with `%age% = 18' tests), `onchange' on a numeric,
# and a user-defined `round' built on `$instr$'/`$left$'/`$mid$'.
play SimPoliticalCareer  "Sim Political Career.asl"    "Sim Political Career - command script.txt"                 "congratulations, Prime Minister" --tick --seed 1

# Dog Catcher (Alexander L. Nielsen, 2009, ASL 400): find the dog Sofus in the
# garden of a house of about 30 rooms.  Open the office cupboard (which reveals
# the biscuits), take them -- an implicit "(first removing them from cupboard)" --
# walk out through the scullery to the flagpole lawn and use the biscuits on the
# dog.  The other win, leash on dog while holding biscuits, is a `$rand(0;4)$ < 3'
# test that otherwise loses, so the script takes the deterministic route.
play DogCatcher          "Dog Catcher.cas"             "Dog Catcher - command script.txt"                          "You've found my dog"

# Caravan Chaos (ASL 400, no author): one caravan room plus a snowstorm maze,
# panes off and a `description' script, so no object or exit lists.  A strictly
# ordered chain: bowl from the cupboard; `examine' bowl / window / memory card
# hand over the coin, card and battery through per-object `action <examine>'
# scripts behind a game-level `verb <examine>'; coin on valve fills the bowl
# (without the bowl held it floods the caravan and loses); `look under bed' is a
# game-level command revealing the box; bowl on box gives the magnet, magnet on
# bed the keys; `out' is gated on the keys.  The maze is e, w, n -- any other step
# reaches `cliff', whose description script is playerlose.  Battery on the engine's
# wires does `goto <roof>', where `wires' resolves to a second object of that
# alias and the coin wins.  The intro's `|w' waits consume no script line.
play CaravanChaos        "caravanchaos.cas"            "Caravan Chaos - command script.txt"                        "Awesome!"

# A Hitmans Life (QDK Lite 4.04, four rooms): fetch the hand gun from the weapons
# room and use it on the Presadent; the Ak47 is playerlose.  The gun object is
# named `Hand gun.' with a trailing full stop: `take hand gun.' works, but `use
# hand gun. on presadent' is refused in Quest and here alike, so the script uses
# the dotless form.  The marker "you shoot holamoladola" is the win's own line,
# distinct from the Ak47's "you shoot at Mr holamoladola".
play AHitmansLife        "a hitmans life.asl"          "A Hitmans Life - command script.txt"                       "you shoot holamoladola"

# The Bomb (bomb.asl, beta 0.1) -- NOT a win: a two-room timer toy with no
# playerwin, whose only ending is playerlose (standing in the Bomb Room at
# detonation).  The script runs the whole survivable cycle: arm the bomb (the
# `enter' prompt takes `3', which `set interval <Timer; #time#>' turns into the
# fuse), `go to escape room' (movement is by `place'), sit out the explosion, wait
# for the 30-second New Bomb timer and go back to look at the fresh bomb.  The 31
# `look's are the turns that timer needs, not padding.  --tick is mandatory.
play TheBomb             "bomb.asl"                    "The Bomb - command script.txt"                             "a new bomb comes out of nowhere" --tick --seed 1

# The Bomb 0.2 -- NOT a win, and it ends in playerlose.  In 0.2 you must carry
# the bomb to arm it, and dropping an armed bomb is `$rand(0; 1)$' with 1 fatal;
# at seed 1 that draw is 1 in Quest and here, and nothing else draws, so
# surviving is out of reach at this seed.  The script takes the seed-independent
# ending instead: take, `set bomb' (a `define synonyms' for arm), answer 3, and
# hold through the 2, 1, 0 countdown to the `lose' text.  --tick is mandatory.
play TheBomb02           "The Bomb 0.2.asl"            "The Bomb 0.2 - command script.txt"                         "you got hit by the bomb" --tick --seed 1

# Digimon (QDK Lite 4.02, six rooms) -- no playerwin; the author's ending is the
# arena's room-level command `use blue card on digivice', then `finish', which
# runs `stop'.  That command matches the literal text and checks no inventory, so
# the card and digivice pickups are story, not requirements.
play Digimon             "digimon.asl"                 "Digimon - command script.txt"                              "you killed the evil digimon"

# Easter Day -- no playerwin; the final room `Outside' prints "You made it!" and
# runs `stop'.  Key from the parents' room, `use key on case' shows the Map,
# taking it shows the Little box, opening that shows Another key, and `use another
# key on outside door' does `create exit northwest <Hallway; Outside>'.  Leans on
# objects parented in closed containers, an `action <take>' overriding the
# game-level `verb <take>', an `open' script, and a runtime-created exit.
play EasterDay           "Easter Day.asl"              "Easter Day - command script.txt"                           "You made it!"

# Firebird Island -- NOT a win: a six-room QuestNet scenery sandbox (it includes
# the absent net.lib and loads without it) with no ending of any kind.  The
# script is a full tour: every room, all four custom verbs (property-as-verb
# `Read=' / `pet=', game-level `smile' and `kiss'), `alt' names, and the one
# takeable object, ending on the bed description that only the last room prints.
# The porch has two objects aliased "Rocking Chair", so `look at rocking chair'
# raises a disambiguation menu, which script line 7 (`1') answers.
#
# Fenom Online, the other QuestNet game in the corpus, has no script: five empty
# rooms with no exits, objects or commands, and no playerwin, playerlose or stop.
play FirebirdIsland      "firebirdisland.asl"          "Firebird Island - command script.txt"                      "elegant oak construction"

# Do What You Want -- NOT a win: a sandbox with one "mission" and no playerwin
# (only playerlose).  The script ends at the last scripted event, arresting
# Killer, where the game says "You've done the only mission in the game!" and,
# after a `wait', the marker.  Every exit but up/down is a `place' exit, so
# movement is `go to <alias>' with the room's article ("go to your bedroom");
# "go to bedroom" fails.  Joining the police is a `choose' menu on `speak'
# (option 1); the arrests are custom-verb `action <arrest>' scripts that `goto'
# the station.  No step is flag-gated -- they are kept so the script follows the
# story.
play DoWhatYouWant       "do what you want.asl"        "Do What You Want - command script.txt"                     "You can now do what you want"

# Sandbox Samurai: collect the DVD shuriken, the mousechuk and the wakazashi,
# then `use sandbox'.  `open scrapbooking kit' is essential: the kit, scissors
# and tape live in a dummy room `objroom', `look under bed' hands over only the
# kit, and the scissors and tape can never be taken -- but once the held kit is
# open, `cut dvd with scissors' works and `if got <tape>' is true (the container
# rule; real Quest agrees).  Mom leaves her room only after both the sink and the
# bathtub are turned on; the DVD is `hidden' inside the closed player until
# `open'.
play SandboxSamurai      "sandboxsamurai.asl"          "Sandbox Samurai - command script.txt"                      "Congrats"

# Lands of Unknown (ASL 350): many rooms run `script playerlose' on entry.  The
# one winning route is left door, portal (`go to hill side'), key from the ship,
# `use key on crate' (Blue pendant), `use blue pendant on volcano stream'
# (`create exit east'), north-west, east, `examine skeleton' (the gun), `use gun
# on dragon'.  The gun also carries `use on anything' ("no more ammo"), but the
# dragon's own `use <gun>' takes precedence.  Spending the key on the forest
# chest instead of the crate makes the game unwinnable.
play LandsOfUnknown      "Lands of unknown.asl"        "Lands of Unknown - command script.txt"                     "Your wedding is in Canada"

# Treasure Island (treasure hunt quest grace.asl; not the corpus's "Treasure
# Hunt") -- NOT a win, and unwinnable by a game bug: the only playerwin is the
# room script of `F,10', and no exit or goto anywhere leads there.  The script
# solves both monster puzzles on the 10x10 coordinate grid -- jam sandwich to the
# white monster at F,3 (menu option 1), ham sandwich to the black monster at I,8
# (option 2) -- and steps south through the unlocked exit into I,9, whose
# description is the marker.  The monster rooms run `choose' from the room script
# and lock/unlock exits.  The `Lives' variable's `onchange' fires on any change,
# so the health pickup at G,5 (`inc <lives>') prints "You lose a life." in Quest
# and here alike.
play TreasureIsland      "treasure hunt quest grace.asl" "Treasure Island - command script.txt"                    "This is where you come out of a portal"

# The Battle (battle.cas): script line 1, "Sam", answers the start script's
# `enter <name>'.  The balloon is filled with `use balloon on water' -- the
# handler is on the water, so the reverse order is refused.  Bob's `ask' takes
# `1'.  The `east' exit of `main hallway a' is a script that, with the lab key,
# does `create exit east', so `east' is typed twice.  `drink holy water' opens a
# menu (option 2 swaps holy water 1 for 2); once the vial is held "water" is
# ambiguous, so the script says `use bottle on teacher'.  Menus: riddles 2 and 2,
# god of death 2.  A non-numeric line at a menu is read as option 1.  No --tick:
# timer `dog' is never enabled and `timeron <school yard>' names no timer.
play TheBattle           "battle.cas"                  "The Battle - command script.txt"                           "Now go tell all your friends"

# Nearco 3 (Jhames, 2009, Spanish, ASL 400) -- the sequel to Nearco, and unlike
# it fully winnable: being ASL 400 rather than 410, the `create exit' rule that
# walls off Nearco 1 does not apply, and both `create exit's here add a direction
# the room never declared.  The script is CP1252, like Nearco's (`examine señora',
# `use arco on jabalí').  A long fetch chain: the beach's "objeto raro" appears
# only after the path has been visited; `poner bolsitas' works only at the temple
# entrance; `use trenza de cabellos on rama' makes the bow (the rama declares the
# `use'); the old man appears only when you carry the colmillos and wants
# colmillos, carta, then the object; `take barca' with the machine wins.  The safe
# is `gira combinacion 1 a la derecha' / 5 left / 9 right, through a synonym table
# mapping `a' to `to'.  Seed-dependent: the brothel's room script draws
# `$rand(1;2)$' on each entry while Meretricia is there, and the safe refuses to
# work until a 1 takes her away -- at seed 1 that is the second re-entry, hence
# `e'/`w' twice.  Four `wait's consume no line.
play Nearco3             "Nearco3.cas"                 "Nearco 3 - command script.txt"                             "Has terminado Nearco 3" --seed 1

# Holes the Game -- NOT a win: an unfinished fragment with no playerwin.  Taking
# the sneakers does `goto <court>'; `speak to judge' raises a `choose' menu and
# `1' picks Camp green lake, whose choice runs `goto' before `msg', so the tent
# description prints before "You have chosen...".  The marker is the camp's only
# scripted event, `sleep on bed'.  The Jail branch dead-ends in a playerlose.
play HolesTheGame        "holes the game.asl"          "Holes the Game - command script.txt"                       "wake up next morning"

# House Adventure -- NOT a win: a four-room sandbox with no goal.  The script
# tours every custom verb once (each a game-block `verb <x>' answered by an
# object `properties <x=text>') and ends on the bath's `get in' reply.  Two lines
# deliberately show game bugs, refused in Quest and here alike: `press power
# switch on computer' and `lay on bed' are shadowed by the shorter verbs `press
# power switch' and `lay'.
play HouseAdventure      "house adventure.asl"         "House Adventure - command script.txt"                      "you get in the bath"

# Incident of the Undead Part 2 -- NOT a win: one room, a seven-way quiz answered
# by `command <A>'..`command <G>', with no playerwin.  Six answers are prose
# deaths; C survives and says `type "win" to win', but `command <win>' is only a
# `playmp3' and prints nothing.  The marker is the last line of the C branch.
play IncidentUndead2     "Incident of the Undead Part 2.asl" "Incident of the Undead Part 2 - command script.txt"  "Time to nut up or shut up"

# Incident of the Undead Part 3: three rooms of multiple choice by `command',
# every wrong answer a playerlose; `b' in the last room is playerwin.  `g' runs
# msg / wait / playwav / msg / wait / wait / goto in one turn (the waits consume
# no line).  In Your Old House the question is only printed by `down', so the
# script types that first.
play IncidentUndead3     "Incident Of the Undead Part 3.asl" "Incident of the Undead Part 3 - command script.txt"  "drift asleep"

# Latrix War Forever -- NOT a win: an abandoned opening with two defined rooms;
# the Labrinth's other nine exits name rooms that do not exist.  The game is
# almost silent, in Quest too: the author wrote `speak' where `msg' was meant
# (`description speak <...>'), so no room prints a description.  The marker is a
# stock string and weak, but it proves location: the Labrinth has exits in all ten
# directions, so "You can't go there." can only come from Team Legends Base.
play LatrixWarForever    "Latrix War Forever!.asl"     "Latrix War Forever - command script.txt"                   "You can't go there."

# TimeRift: `define game <>', so no title and no banner.  A real playerwin (`use
# radio on pedestal of time'), but ungated -- the Zombie can be walked past, and
# `use spear on zombie' prints two "You killed the Zombie" lines without hiding
# it, its test being an unfilled QDK placeholder (`use on 'OTHER OBJECT NAME'').
# The script kills it only because the intro asks.  It avoids the picture `look's.
play TimeRift            "TimeRift.asl"                "TimeRift - command script.txt"                             "farewell, traveler"

# The Tavern: the Victory! room's description script is playerwin.  `punch
# bookshelf' raises a disambiguation menu with two identical "a bookshelf"
# options, the bedside table carrying `alt <bookshelf>' too; `2' picks the real
# one.  Nothing is gated -- `use book on door' and `use gun on pirate' test
# nothing -- but the script plays the intended sequence.  The key is `invisible'
# and arrives by `give <key>' in `search box'.
play Tavern              "the tavern.asl"              "The Tavern - command script.txt"                           "You become a hero"

# On Time (ontime.asl, ~90 rooms): a bride wakes hungover on her wedding day and
# must reach the chapel.  No playerwin: every ending is a `msg <...YOU WIN!>' or
# `<...YOU LOSE!>', a score line, a credits prompt and `stop'.  The script reaches
# the "early" biker ending.  --tick is mandatory: `get up' starts chained timers
# (`early' 540s, `on time' 120s, `late' 120s) that pick the ending, `drink' starts
# `hungover' then `fallover' (a loss, cancelled by `eat cake'), and a 20s `bus'
# timer fires an `enter' prompt mid-turn in `main road'.  So the filler counts are
# exact: 17 `look's wait for "A bus arrives. Get on it?" and 21 for "Get off at
# next stop?"; changing the turn count before the main road means re-tuning the
# first.  Other lines eaten by prompts: `2' (elf menu), `yes', `3' (dress), `left',
# `yes', the bike chase `right' / `straight on' x3, and `yes' for the credits.
# The script never takes the mobile phone or enters the lounge, whose timers would
# inject prompts of their own.
play OnTime              "ontime.asl"                  "On Time - command script.txt"                              "YOU WIN!" --tick --seed 1

# Bob's Adventure -- NOT a win: a six-room sandbox with no ending.  The game
# block's `description font <WP Phonetic>' is a global room-description script
# that only sets a font, so no room ever prints a description and movement and
# `look' are silent, in Quest too.  Custom verbs take the form `<verb> <object>'
# (`go your gun', `pick up lady').  The script tours the six rooms, fires every
# verb that works in real Quest and ends on `kill lady'.  It avoids the objects
# named `him' and `Her' (Quest reads those as pronouns), the Airport's `place'
# exits (shadowed by the game's own `Go to' verb), and the gangster's `Waste the '
# property with its trailing space.
play BobsAdventure       "bobsadventure.asl"           "Bob's Adventure - command script.txt"                      "Suddenly you her cops coming"

# Lost Jasmine: a chain of hidden objects revealed by `show' -- `open sirap' shows
# Yago, `speak to yago' the Stable, `look at stable' the Key -- and Key is a
# `container' whose `open' script is playerwin.  The Hazrat / policemen branch is
# optional and ends in playerlose.
play LostJasmine         "lostJasmine.asl"             "Lost Jasmine - command script.txt"                         "she is ready to go with you"

# Motorbike Keyfinder -- no playerwin, but the intended ending: `look at
# computertable' shows the cloth, `take cloth' is an `action <take>' that shows
# the hidden nail instead of taking anything, and the nail is a `container' whose
# `open' script prints the congratulations.  The game keeps running afterwards.
play MotorbikeKeyfinder  "Motorbike Keyfinder1.2.asl"  "Motorbike Keyfinder - command script.txt"                  "You have found your Motorbike key"

# Math Water Magic Journy (Water-venture.cas, a school project) -- NOT a win: the
# game is a loop, the last room (School) printing its text, `wait', then `goto
# <Your room>'.  The marker is the School text.  The maths answers are synonyms
# (`96 = go to bathroom2', `12', `28'), and the exits they use are made at run
# time by `create exit', so `use sink' must come before `96'.  Every room has
# `beforeturn clear'; `wait' + `clear' + `goto' in description scripts consume no
# line.
play MathWaterMagicJourny "Water-venture.cas"          "Math Water Magic Journy - command script.txt"              "make a etxt based adventure game"

# Hungry Goblin (ASL 350) -- a real playerwin, but only through a game bug.
# Numeric `tummy' starts at 95, `afterturn' adds 1 a turn, playerlose at 100 and
# playerwin below 0, and all the food together is worth 98, so it cannot be won
# honestly (the intended tavern ending is an empty room).  Eating the venison only
# does `hide <venison>' while it stays held, and `examine campfire' always does
# `show <venison>', so the pair repeats for a net 28.  The worm must be eaten
# within the first four turns.  `eat #@food#' is a custom command.
play HungryGoblin        "hungry goblin.cas"           "Hungry Goblin - command script.txt"                        "You are finally full"

# Beyond Manage: a true win (playerwin at the end of Stage 1, Lay Vega Room 10;
# the Venus Rogue rooms are empty stubs).  Needs --tick: the tutorial is gated
# by two timers.  The filler turns are `look at ...', not `look', because the
# room description calls timeron again and would restart the timer.
play BeyondManage "beyondmanage.asl" "Beyond Manage - command script.txt" "end of Stage 1" --tick --seed 1
# Dungeon (ASL 350): NOT a win, the game is unwinnable as written.  The first
# Maze room has no west exit although the map says to go west, so everything
# beyond it, the playerwin room included, is unreachable.  The script stops at
# the last reachable scripted event, the map hint.  Needs --tick: the Dungeon
# Master only appears after 20 turns in the exitless cell.
play Dungeon "dungeon.asl" "Dungeon - command script.txt" "The map says you should go west" --tick --seed 1
# Realm of Chaos Demo: no playerwin; the demo ends with "You have completed the
# demo!" and a stop.  The thief's speak script has a stray second `enter', so
# the password line appears twice.
play RealmOfChaosDemo "realm of chaos 1.0 demo.asl" "Realm of Chaos Demo - command script.txt" "You have completed the demo" --seed 1
# Enterprise (ASL 350): NOT a win, a sandbox with no goal and no playerwin.  The
# script follows the one scripted storyline to its end, a cell with no exits.
# The game includes q3ext.qlb, which is bundled (FINDINGS 83); its clothing and
# room descriptions are the library's.
play Enterprise "enterprise.asl" "Enterprise - command script.txt" "you see several other cells" --seed 1

if [ -n "$only" ] && [ "$matched" -eq 0 ]; then
    echo "no game label matches --only '$only'"; exit 1
fi
[ "$bless" = yes ] && exit 0
echo "----"
echo "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
