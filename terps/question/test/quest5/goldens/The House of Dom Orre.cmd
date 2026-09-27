# The House of Dom Orre — ASL 550, standard parser; no published walkthrough
# exists — derived entirely from game source (game.aslx, 471 objects). A
# reporter investigates the vanished eccentric scientist Dom Orre, exploring
# his house, dungeon and garden to collect all nine of his fabulous
# artefacts (coin, bat, pearls, pen, chest of jewels, jade kiwi, toad, egg,
# fly), then drives off through the garage. Win: with the ignition key
# inserted, `press button` (or `push button`) in the car — the script also
# nets the two optional bonus flags (reading the diary for the "fate of Dom
# Orre", and running the laboratory experiment to synthesise the diamond) for
# the best ending: "You found all the valuable artefacts! Well done - your
# editor will be pleased." 168 commands, errors=0, state=Finished.
#
# Engine gotchas hit while deriving this (some corpus-wide, not
# game-specific):
#  - `hidechildren` containers whose contents are revealed by a CUSTOM script
#    need an explicit `look at <container>` before the newly-visible child is
#    name-resolvable: the mud (ring), the small blue jug (ignition key), the
#    hole in the furnace-room wall (white object, after `touch web`), the
#    bench (box/flask/powder/tweezers), the cistern (ball cock, after
#    `lift lid`), and the freshly-placed open safe (egg, after the loft
#    ball-cock puzzle). This does NOT apply to the engine's default
#    open/take handling (e.g. `open Oliver Twist` auto-lists the brass key,
#    `open wardrobe` auto-lists the hand/skeleton) — only to containers whose
#    reveal happens via a hand-written script that merely flips visibility
#    without also calling the engine's own content-lister.
#  - The slipper is a special case of the same gotcha: `feel in slipper`
#    only sets the jade kiwi's own `visible` flag — the slipper's
#    `hidechildren` gate (which blocks ALL of its children regardless of
#    their own visibility) is only cleared by a subsequent `look at slipper`.
#  - `unlock`/`lock` take no "with X" clause — the correct key is
#    auto-detected from inventory.
#  - The RNG-gated "lose ring" turnscript (~0.5%/turn once the ring is worn)
#    silently drops the ring in the player's current room with a "You hear a
#    clink." cue and clears its `worn` flag; without the ring worn the
#    phantom-attack turnscript's RandomChance death trap becomes live after
#    turn 10. This walkthrough hits the drop once (deterministically, given
#    the oracle's fixed seed and this exact command sequence) and immediately
#    re-`take`s/`wear`s the ring in the same room before moving on.
#  - The underground-cell toad is guarded by the straw's `search`/`look
#    under` script, which explicitly refuses ("Nothing there.") whenever a
#    `lying man` is present in scope — so clubbing/hitting the dungeon
#    "monster" (which always leaves a `lying man` behind) permanently blocks
#    the toad. The non-violent solution — `give glass to man` (the glass of
#    corrosive kitchen liquid, carried down after cleaning the guard room's
#    rusty key in it) — makes him drink and flee entirely, leaving the cell
#    empty and the toad findable.
#  - The stepladder can only be climbed to the loft while NOT held and while
#    standing on the landing (`climb stepladder` checks `not Got(stepladder)`
#    and `parent = landing`), so it must be carried there and dropped first.
#  - The diamond-experiment box (laboratory) refuses to open while still
#    switched on (it just re-closes itself with "It won't open."), so the
#    countdown must be given time to finish (any number of turns — this
#    script interleaves ~100 turns of other business) and the box must be
#    switched off again before it can be opened to retrieve the diamond.
#  - The treasure chest's typed name for the parser is its `alias`,
#    "chest full of jewels" — not the object's internal name "treasure
#    chest" (`take treasure chest` fails with "I can't see that.").
#  - Avoid `ring the enormous bell` (belfry) and `eat bread roll` while still
#    `infected` (salmonella) — both are unconditional death traps; this
#    script washes hands (clearing `infected`) at the shower room early on,
#    long before the bread roll is eaten, and never touches the bell.
#
look
look at mud
take ring
wear ring
open small door
south
open Oliver Twist
take brass key
west
look at small blue jug
take ignition key
open wardrobe
lift hand
take diary
read diary
take pen
east
east
wash hands
west
north
unlock sliding door
open sliding door
west
look at furnace
touch web
look at hole
open furnace
north
north
look at bench
take flask
take small pile of black powder
take pair of tweezers
open box
put flask in box
put small pile of black powder in flask
close box
turn on box
south
south
use tweezers on white object
break cocoon
take fly
east
up
north
look at mantlepiece
open tobacco jar
take tobacco
take box of matches
take pipe
put tobacco in pipe
light pipe
open cupboard
take screwdriver
feel behind cushions
take coin
press bricks
in
press button
east
take rubbish
take bat
west
press button
out
south
west
look at table
take glass
north
take bread roll
eat bread roll
south
east
south
east
look under bed
take slipper
feel in slipper
look at slipper
take jade kiwi
west
south
west
smoke lit pipe
unscrew vent
take pearls
east
south
kf3
west
down
north
north
look at table
take rusty key
put rusty key in glass
wait
take shiny key
south
west
unlock door
open door
give glass to man
in
search straw
take toad
out
west
up
out
south
south
in
take spade
take stepladder
out
north
east
dig
dig
dig
dig
take chest full of jewels
north
west
in
down
take ring
wear ring
east
east
south
up
east
north
north
north
drop stepladder
climb stepladder
south
lift lid
look at cistern
twist ball cock
twist ball cock
twist ball cock
lift ball cock
press ball cock
look at open safe
take egg
north
down
down
west
north
north
turn off box
open box
take diamond
south
south
east
north
in
insert ignition key
press button
