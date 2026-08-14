#!/bin/sh
# Play a collection of Quest games against their walkthroughs and print a
# PASS/FAIL table.  The games are copyrighted, so they are kept local-only in
# ../games (gitignored, see ../../.gitignore); the games dir defaults there but
# can be overridden.
#
#   ./run_walkthroughs.sh [games dir] ["extra walkthroughs dir"]
#   ./run_walkthroughs.sh --bless [dirs...]     (re)record the transcripts
#   ./run_walkthroughs.sh --win-only [dirs...]  win markers only, no diffing
#
# Each game is checked twice over: the win marker has to appear, *and* the whole
# transcript has to match "<title> - transcript.txt" in ../goldens byte for
# byte.  The marker alone is a weak test -- an engine change that garbles every
# room description still ends the game, so it still passes -- and these games
# between them exercise far more of the engine than the fixtures can.
#
# The transcripts are ours to record but the walkthroughs behind them are not
# all ours: the three "<title> - walkthrough.txt" files are other people's work
# and are not redistributed here (point the second argument at a directory
# holding them to include those games, otherwise they SKIP).  A transcript
# spells out the commands it replays, so recording one would republish the
# walkthrough -- those three games stay win-marker-only.  A walkthrough is
# looked up in ../goldens first and in that directory second.
#
# Builds the runner if needed.  Uses a fixed RNG seed for reproducibility;
# World's End's two random fights are won by --save-scum regardless of seed.
# The runner substitutes its own generator for the C library's rand() so that
# the seeded draws -- and so the transcripts -- are the same on every platform;
# see the comment in geas_walkthrough_runner.cc.

set -u
here=$(cd "$(dirname "$0")" && pwd)

bless=no; winonly=no
while [ $# -gt 0 ]; do
    case "$1" in
        --bless)    bless=yes; shift ;;
        --win-only) winonly=yes; shift ;;
        *)          break ;;
    esac
done

G=${1:-"$here/../games"}
W=${2:-}

# Where does this walkthrough live -- in the repo, or in the caller's own dir?
wtpath() {
    if [ -f "$here/../goldens/$1" ]; then printf '%s\n' "$here/../goldens/$1"
    elif [ -n "$W" ] && [ -f "$W/$1" ]; then printf '%s\n' "$W/$1"
    fi
}

RUN="$here/geas_walkthrough_runner"
# Always ask make: the runner unity-includes the engine sources, so a binary that
# is newer than geas_walkthrough_runner.cc can still be older than the edit being
# tested.  The Makefile depends on ../*.cc and ../*.hh, so this is a no-op when
# nothing has changed.
make -C "$here/../.." >/dev/null || exit 1

tmpdiff=${TMPDIR:-/tmp}/geas_walkthrough_diff.$$
trap 'rm -f "$tmpdiff"' EXIT INT TERM

export GEAS_SEED=1
pass=0; fail=0

# play  <label>  <game>  <walkthrough>  <win-marker>  [extra runner args...]
play() {
    label=$1; game=$2; wt=$3; marker=$4; shift 4
    wtfile=$(wtpath "$wt")
    if [ ! -f "$G/$game" ] || [ -z "$wtfile" ]; then
        printf '%-22s SKIP (missing files)\n' "$label"; return
    fi

    # Only the scripts in ../goldens get a transcript; see the header.
    golden=""
    case "$wtfile" in
        "$here/../goldens/"*)
            golden="$here/../goldens/${wt% - command script.txt} - transcript.txt" ;;
    esac
    [ "$winonly" = yes ] && golden=""

    got=$("$RUN" "$@" --echo --win "$marker" "$G/$game" "$wtfile" 2>/dev/null)
    won=$?

    if [ "$bless" = yes ]; then
        if [ -z "$golden" ]; then
            printf '%-22s skipped (no transcript for this one)\n' "$label"
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
play BrokenMirror        "BMTSFD.asl"                  "Broken Mirror - The Screaming Fountain - walkthrough.txt"  "completed the rather short demo"
play FadeToWhite         "White.asl"                   "Fade to White - walkthrough.txt"                           "END OF DEMO"
play Koww                "KOWW1.ASL"                   "Koww the Magician - command script.txt"                    "over the chasm"
play MagicWorld          "Magic.asl"                   "Magic World - command script.txt"                          "won the game"
play SirLoin             "sirloin.cas"                 "Sir Loin and the coming of age - command script.txt"       "To be continued"
play Space               "space.asl"                   "Space - The Final Fuck Up - command script.txt"            "pint or two"
play Uranus              "uranus.asl"                  "Uranus or Bust - walkthrough.txt"                          "BOOOOOM"
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
play WorldsEnd           "worldsend/world's end.asl"   "Worlds End - command script.txt"                           "slumps to the ground dead" \
    --tick --save-scum \
    --fight "use vial1 on cube|use vial2 on cube=The cube explodes" \
    --fight "use laser pistol on warlord=slumps to the ground dead"

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
# abide; you win by holding the yellowy blob and dropping it in his chamber.  It turned up
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
# Note the `2` answers: geas renders `ask` as a 1)Yes 2)No menu read with atoi, so a spelled-out "no"
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
# from `startscript`, which is 6808 under GEAS_SEED=1 (the mirror in the tower of light reports it).
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
# as "Yep, 100% empty." and geas threw out of GeasFile::static_eval on the unpaired percent sign;
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
# (SetUnknownVariableType, V4Game.Part2.cs:592-618); geas did not, so the reset
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
# _objs, so it never confuses the two; geas resolved the name to the room, which
# printed the room's look text for `x desk`, pasted the room's listing prefix onto
# the object, and made `here <desk>` false so the library refused to hand anything
# over.  An unqualified lookup now answers for the object and the room half is
# named explicitly, including through the deprecated room-scoped `gettag`
# (V4Game.cs:6899), which geas did not implement and which this library needs both
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
# it never is (ExecUse, V4Game.Part2.cs:5348-5368); geas checked the room first at
# every version.  This game is 350 and needs the room to win twice, since the
# vacuum cleaner's own use only comments on the noise while the two rooms that
# matter use it to uncover the Bob-omb doll and the door to Lady Bow -- see
# fixtures/bareuseroom.asl and bareuseroom410.asl.  Both endings call `playerwin`;
# this route takes the one the author labelled "REAL" (and then announced as the
# secret one, which is where the win marker comes from).
play MarioIsMissing2     "MIM2UQ.asl"                  "Mario Is Missing 2 Ultimate Quest - command script.txt"    "Congratulations on finding the secret ending!" --tick

# Shipwrecked is the corpus's big QDK treasure hunt: three islands, 419 turns,
# four scripted fights won with fixed menu answers under GEAS_SEED=1, and a 3x3
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
# onto whatever is added to it (V4Game.cs:2117-2121), so geas now walks the chain
# the way `here` already did (fixtures/gotcontainer.asl).  Note the deliberate
# `look at <container>` before every `open <container>` in the script: the game
# defines its own `verb <open>`, which bypasses the built-in open command and so
# never marks the container "seen" -- and an unseen container keeps its contents
# out of scope.  That is faithful, not a bug.  All three endings call `playerwin`;
# this route takes ending1 at 30/400, since the richer endings want every treasure
# banked in the sail boat's glass case.
play Shipwrecked         "Shipwrecked1.3.cas"          "Shipwrecked - command script.txt"                          "You have completed Shipwrecked"

# Barbarian is the corpus's longest fight: 633 script lines, 508 turns, 250/250,
# and eight scripted battles plus a five-stage trap all won with fixed menu
# answers under GEAS_SEED=1.  Every optional fight is compulsory, because a win
# is +1 maxhealth and Gak only teaches the Vault above 16 -- and the Vault is the
# one thing in `thorgincombat` that clears `enemymounted`, so without it the
# final duel never leaves its mounted loop.  Two divergences came out of it, both
# fixtured.  The game's own `unwield` command computes
# `$left(#command1#; %length1%)$` over "Battle axe (wielded)" to strip the
# " (wielded)" suffix; geas ended the argument list at the *first* right paren,
# which cut it down to one argument, so the function returned nothing and the put
# away weapon was never handed back -- Quest's DoFunction takes the last paren
# (InStrRev, V4Game.cs:6745), fixtures/funcparen.asl.  And Quest records
# containment as the child's own "parent" property, which games read back as
# `#child:parent#` (DoAddRemove, V4Game.cs:2113-2133); geas kept containment only
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
# (V4Game.Part2.cs:4149-4163).  geas does the same; the live routes are USE CAR
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
# suite's GEAS_SEED=1 matters here.  The Enlighten spell is only unlocked by
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
# geas used to take a parented object without clearing its `parent`, so pulling
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
# because of geas's faithful `seen` gate (a container's contents stay out of
# scope until the container has been examined -- see fixtures/containervis);
# LOCK DOOR after the control room is required, since Refinery Main Floor's
# entry script raises the chain hoist again if `chainlift1` and `kp11` are both
# set; and the bare LOOK in the Secret Labratory is a sacrificial turn, because
# that room's `beforeturn` is `if here <Dave> then dontprocess`.
#
# No new engine divergence came out of the second half of the game.  It did
# confirm empirically that geas keeps `invisible`/`conceal`ed objects in scope
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
# the chain.  geas used to stop at the room, so USE IMMUNE POTION answered with
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
# geas already gets this right, and no divergence came out of the game.
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
# re-entrancy guard (V4Game.Part2.cs:526-530) and would blow its stack; geas
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
# turn one; geas used to hide it, which made the Drink of Immortality unmakeable
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

# Metal Sonic's Quest is the game that found the `gametype multiplayer' bug: geas
# used to throw "Error: geas is single player only." from the middle of the loop
# that reads the game block, which also skipped the `start' line below it and
# left the player in room "" with all 148 rooms in scope.  Quest never reads the
# declaration (V4Game.Part2.cs:7992-7999 scans the block for "start " and
# nothing else), so geas now ignores it; see fixtures/multiplayer.asl.  --tick is
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
# only works because of it.  geas used to search both scopes at once and raise a
# disambiguation menu Quest never shows, silently eating the next script line.
# (c) Trailing spaces in object names: `define object <Journal >' handed over as
# `give <Journal >', plus two bricks aliased `The Brick ' and `The Brick  '.
# Note `take iguana', not `take iquana': the author's `define object <iquana>' is
# aliased `iguana', and Quest matches aliases only (V4Game.cs:4704-4727).  Every
# `go to' line is a room ALIAS, per the ASL >= 311 `PlaceExist' rule.  No
# `playerwin' anywhere -- the marker is the congratulation `choose <octoberiffic>'
# prints in its `finalscene' branch after you press SEND on the last MSN message.
play SomethingBoutAHex   "something 'bout a hex.cas"   "Something 'Bout A Hex - command script.txt"                "completed BOOK TWO of Something 'Bout a Hex" --tick

[ "$bless" = yes ] && exit 0
echo "----"
echo "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
