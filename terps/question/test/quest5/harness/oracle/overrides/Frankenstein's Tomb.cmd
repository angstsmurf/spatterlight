# Frankenstein's Tomb (Craig Dutton, 2017, ASL 550). No published walkthrough
# exists anywhere; this 112-step script was derived from the game source. You
# play Frankenstein's creature, and it reaches the game's GOOD ending (source
# line 2292): Viktor is present when the crystal beam ignites the gunpowder, so
# he shoves you clear and dies in your place. The other `finish` (line 2304) is
# the same explosion with Viktor absent -- "You have failed to achieve your true
# fate!" -- and is what you get if you set the cask off on your first visit.
#
# Route notes, all verified against the source:
#  - `board ship` is an authored bug: the deck's own command runs
#    MoveObject(player, ships deck), and no such object exists, so it raises
#    "Unknown object or variable 'ships___SPACE___deck'". Both ships must be
#    entered by their exits instead -- `go to deck of the ship` / `go to deck`.
#  - The blizzard north of the thermal springs is the one RNG gate in the game:
#    the exit's firsttime always pushes you back, and every attempt after that is
#    a RandomChance(20) (line 1622). Under the harness seed (1234) it clears on
#    the 25th attempt -- and clearing it does NOT move you, the script only sets
#    blizzard.cleared, so a 26th `north` is needed to actually reach the chasm.
#    Hence 26 norths at the springs. Re-seeding changes only that count.
#  - The alien city is a one-way drop (`jump in chasm`); the chasm cannot be
#    climbed. The ONLY way out is the ice-hall dais, which teleports you to the
#    fogbank once crystal chamber.citypoweron is set -- i.e. you must solve the
#    crystal chamber before you can leave, then come back a second time.
#  - Button order comes from the loose strip of metal in the ice hall: "yellow,
#    green, blue and red from left to right". The dais is `hidechildren`, so
#    `x dais` is what brings the silver cube into scope.
#  - The obelisk is two steps: `push obelisk` fells it, and only then does
#    `x base` (firsttime) reveal the glowing crystal. The stone block it leaves
#    behind is the only thing that breaks the log cabin's trapdoor.
#  - First visit to the crystal chamber: `look behind console` (firsttime)
#    reveals the recess, the glowing crystal in it powers the console, `turn
#    wheel` opens the dome (sunlight) and `pull lever` opens the pit. Pressing
#    the four buttons then latches citypoweron. `push lever` immediately after
#    closes the pit and stops the rod glowing, which is what makes it safe to
#    carry the cask back in later -- the ending turnscript fires the instant rod
#    is glowing AND the pit is open AND the pit holds the cask. Take the crystal
#    back out: the cargo hold is dark and it is the only lightsource.
#  - Outside chores: `use silver cube on waters` at the thermal springs melts the
#    cube down to the silver key (for the iron door), `cross ice bridge` triggers
#    the avalanche, and `dig avalanche` needs the igloo's shovel.
#  - Second visit: `pull lever`, drop the cask in the pit -- that is what the
#    frankensteinarrives turnscript watches for, so Viktor walks in on the same
#    turn -- then re-press yellow/green/blue/red. The rod lights, and blowntohell
#    fires with Viktor in ScopeVisible(). The crystal is no longer in the recess
#    at this point and does not need to be: citypoweron is already latched.
#
# Deterministic under seed 1234 (the blizzard is the only RNG); no score.
# state=Finished, errors=0, 112 steps.
x me
go to deck of the ship
x dead bodies
go to trapped ship
east
north
east
in
take shovel
out
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
north
jump in chasm
north
in
x dais
take silver cube
take metal
read metal
out
north
northeast
push obelisk
x base
take glowing crystal
take stone block
southwest
in
up
x console
look behind console
put glowing crystal in recess
turn wheel
pull lever
push yellow button
push green button
push blue button
push red button
push lever
take glowing crystal
down
out
south
in
stand on dais
north
east
north
use silver cube on waters
take silver key
south
west
cross ice bridge
dig avalanche
north
in
break trapdoor
down
north
unlock iron door
open iron door
west
go to deck
down
take cask of gunpowder
up
leave ship
east
south
up
out
south
cross ice bridge
east
north
north
jump in chasm
north
north
in
up
pull lever
put cask of gunpowder in pit
push yellow button
push green button
push blue button
push red button
