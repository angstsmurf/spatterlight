# Fountain of Eternal Youth v1.3 (Quest 5.8.6836.13983, May 2019, ASL 580) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=1, 29 steps -- the game's single ending ("Congratulations!
# You have completed part one of the Fountain of Eternal Youth! ... Part two coming soon!").
# Part two was never released; this IS the whole game.
#
# SOURCE: no walkthrough exists for this game anywhere; derived entirely from game.aslx.
# It is a small, fair, self-contained gem hunt -- three jewels onto three colour-matched
# pedestals opens the steel door and teleports you to the End room, which `finish`es.
#
# THE ONE ERROR IS THE AUTHOR'S, at load time: the red pedestal's `_initialise_` script is
# a bare `LockExit` with no argument -> "No parameters passed to LockExit function".
# It fires once during initialisation, before the first prompt, and has no effect on play.
#
# MAP. Entrance to the Maze --north--> Sandstone Passageway --north--> North Garden, with
# the tunnel network hanging below (Passageway --down--> Worm Tunnel1). Six tunnels, all
# aliased "Worm Tunnel", so the room banner never tells you which one you are in:
#   WT1  fungi1;    south->WT (fall-in room), west->WT5 (DEATH), northeast->WT4
#   WT4  shovel;    southwest->WT1, south->WT6
#   WT6  roots8;    north->WT4, south->WT2, up->West Garden (locked: avalanche of dirt)
#   WT2              northwest->WT, east->WT3, north->WT6
#   WT3  helmet, sword, roots3, fungi3;  west->WT2 (dead end)
#   WT5  INSTANT DEATH -- entering runs `finish`; the purple worm eats you.
#
# KEY DERIVATION NOTES (all verified against game.aslx):
# * The lantern must be BOTH carried and lit before the first `north`. Sandstone
#   Passageway's `enter` only avoids the concealed chute when `IsSwitchedOn(brass lantern)`
#   AND `Got(brass lantern)`; otherwise you slide down into Worm Tunnel. That is survivable
#   (the tunnels are where the tools are anyway) but it costs the safe `down` route and
#   drops you a room away from the fungi, so the script does it properly.
# * `dig dirt with shovel` unlocks the `avalanche` exit up to the West Garden. The shovel
#   carries four synonymous scripts (digwith/digdirtwith/digavalanchewith/shoveldirt), all
#   identical and all gated on `game.pov.parent = Worm Tunnel6`, so the digging must happen
#   in WT6 specifically -- anywhere else prints "There is nothing to dig."
# * The sword is picked up BEFORE cutting: `cut roots with sword` is a scriptdictionary
#   keyed on the sword object, and roots3/roots8 have no other way to be harvested
#   ("The roots are too firmly fixed in the soil."). Either set of roots satisfies the
#   cauldron; roots3 (in WT3, alongside the sword) is on the way, so roots8 is skipped.
# * Cauldron riddle -- "bring me what the small folk eat, and the big folk's mouths": the
#   moth-licked luminescent FUNGI and the ROOTS ("roots" as in a tooth's roots / the big
#   folk being trees). Its turnscript accepts fungi/fungi1/fungi3 x roots3/roots8, i.e. any
#   one fungus plus any one root, and pays the sapphire on the turn both are in the pot.
# * `take plums` raises a four-way disambiguation between plums/plums1/plums2/plums3, four
#   clones that all alias to "plums". The menu is answered with a bare `1`; leaving it
#   unanswered silently swallows the take (the next command runs, the plum does not).
#   Only the first clone is needed -- one plum buys the amber.
# * `give helmet to statue` (West Garden) and `give plums to statue` (East Garden) are
#   giveto scriptdictionaries; the East Garden statue's object name is statue2 but its
#   alias is "statue", so it must be addressed by the alias.
# * The three pedestals are typed differently by the author: the red one inherits
#   `container_open` (so `put ruby IN red pedestal`) while blue and yellow are `surface`
#   (`put ... ON`). Quest accepts either preposition for either, but the script uses the
#   authored forms.
# * Colours match the jewels, per the in-game hints book: ruby->red, sapphire->blue,
#   amber->yellow. The `pedestal puzzle` turnscript then fires on the same turn as the
#   third placement and does `MoveObject (player, End)`, whose `enter` is `finish` -- so
#   no further command is needed and the script ends there.
# * Optional content deliberately skipped: the three bookshelf books (advice/hints/for
#   beginners), the golden tooth and beetle on the skeleton in WT3, the loincloth (taking
#   it destroys it), and every `take dirt`. None of it is scored or required -- this game
#   has no score.
take lantern
switch on lantern
north
down
take fungi
northeast
take shovel
south
south
east
take sword
take helmet
cut roots with sword
west
north
dig dirt with shovel
up
give helmet to statue
northeast
take plums
1
put fungi in cauldron
put roots in cauldron
southeast
give plums to statue
northwest
put ruby in red pedestal
put sapphire on blue pedestal
put amber on yellow pedestal
