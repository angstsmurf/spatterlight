# The Deer Trail (Justin Squires / Dark Forest Media, Ectocomp 2021, Quest 5.8) — winning script
# RESULT: state=Finished, errors=0 — "YOUR HUNT IS OVER" ending (skull given to the
#   lurking shadow; the deer-creature flees through the window), Achievements 17/28.
# SOURCE: "The Deer Blind Walkthrough.txt" (author's own walkthrough — note it is
#   titled after the game's working name "The Deer Blind", not the release name).
#   It is a bare command list, but written in a navigation shorthand the parser
#   does not accept, so the extractor cannot drive it raw.
# Deviations from the raw walkthrough:
#  - Every movement line "Direction to <room>" (e.g. "North to deer blind") -> the bare
#    compass direction: the notation names the destination for the reader; the game
#    only accepts plain directions ("I don't understand your command." otherwise).
#  - "knock arrow" (twice) -> "nock arrow": walkthrough typo; the game defines a
#    "nock" verb on the Arrow, while "knock" matches the generic knock verb and
#    fails ("You can't knock it."), leaving the bow unloaded and the hunt stuck.
#  - "westto deer trail 1b" -> "west", "north bedroom" -> "north": missing-space /
#    missing-"to" typos in the same navigation shorthand.
#  - "Use Stairs to living room" / "use stairs to upstairs hallway west" -> "use stairs",
#    "use steps to backyard" -> "use steps", "enter laundry shoot to cellar" ->
#    "enter laundry shoot": same shorthand on the stairs/laundry-shoot objects; the
#    trailing "to <room>" makes the object unresolvable.
#  - Appended final "quit": the ending prints its closing text but never calls
#    finish (it invites RESTART/UNDO instead); "quit" is the game's only finish
#    channel, so it is what registers state=Finished.
north
nock arrow
rest eyes
shoot deer
northeast
track deer
north
Track deer
north
Track deer
northeast
Track deer
west
Track deer
west
Track deer
north
Track deer
north
northwest
Climb Ladder
Take Rifle
Climb Ladder
southeast
east
east
Take Tackle Box
south
Take ladder
north
west
west
north
Use ladder
west
use stairs
Use rifle
Open cabinet
take call
push cabinet
north
open drawer
take key
unlock tackle box
open tackle box
south
Take arrow
use stairs
east
east
open cupboard
take bottle
enter laundry shoot
Open cabinet
Take compound
take spare parts
use steps
south
open cupboard
take pitcher
north
Use spigot
south
south
Unlock door
south
south
south
south
south
Take Broken rabbit snare
use spare parts on rabbit snare
use rabbit snare
use rabbit call
nock arrow
shoot rabbit
take rabbit
use knife on rabbit
north
north
north
north
north
north
use rabbit meat on meat scale
take key
south
south
south
south
south
east
east
unlock trunk
take sledge hammer
west
west
north
north
east
east
south
use sledge hammer
south
take chemical compound b
north
north
use chemical compound b on mixer
use chemical compound a on mixer
use pitcher on mixer
use mixer
take herbicide
west
west
south
south
east
east
northeast
east
east
east
use spray bottle on vines
enter hole
take key
climb out of hole
west
north
use fishing net on pond
south
west
west
west
southwest
west
north
north
north
north
west
north
use stairs
east
unlock door
north
use skull on lurking shadow
quit
