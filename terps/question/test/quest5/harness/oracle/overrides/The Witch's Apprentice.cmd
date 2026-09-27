# The Witch's Apprentice — Kim Butcher / Eldrichdreams, ASL 540, standard
# parser; no published walkthrough exists — derived entirely from game source
# (game.aslx, 396 objects). A witch's apprentice must brew potions, trade with
# shopkeepers around a fairy-tale village, infiltrate a sorcerer's castle twice,
# and finally break Zandor's magical staff at Madam Ingra's ruined cottage to
# win. Win: `break magical staff` at the charred ruins — "Congratulations, you
# have solved the adventure!" / "The End". 368 commands, errors=0,
# state=Finished.
#
# Engine gotchas hit while deriving this (some corpus-wide, not game-specific):
#  - ShowMenu dialogue choices are answered by typing the bare NUMBER of the
#    option, not its text (Yanda/Madam Ingra/Giant Eagle conversations).
#  - `unlock`/`lock` take no "with X" clause in this engine — the correct
#    key is auto-detected from inventory; just `unlock <object>`.
#  - `throw` is a single-object verb (no "at X" clause) — the target is
#    auto-detected via ScopeReachable(); this script instead uses the
#    equivalent `use pepper on Eft the Fool` (selfuseon), to match the
#    convention used for the game's other combine-two-objects puzzles.
#  - `hidechildren` containers whose contents are revealed by a CUSTOM script
#    (as opposed to the engine's default open/take handling) need an explicit
#    `look at <container>` before their newly-visible contents are
#    name-resolvable: the stone eye's recess (dragon's tooth), the oubliette
#    skeleton's recess (coronet), the trapdoor's recess (gold key), the dusty
#    chamber table (candlestick — also requires taking the candle out FIRST,
#    since the candlestick's key-reveal is gated on the candle being absent),
#    the banquet-hall oak table (salmon), and the island ice pedestal
#    (sapphire).
#  - The toy unicorn's cotton wool is gated behind a true RNG "search barrel"
#    minigame (`GetRandomInt(1,5)`, 1-in-5 per search); this script spams
#    `search barrel` 25 times to reliably land a hit within one run — the
#    replay is deterministic given the oracle's fixed seed, which is all that
#    golden-freezing requires.
#  - Drinking the green potion at the forest ant-hole shrinks the player and
#    drops the ENTIRE current inventory (green potion included) into the
#    forest — everything is recovered afterward with `take all`.
#  - The gigantic emerald in the ant world must be pushed three times
#    (queens chamber -> antechamber -> anthill entrance -> gone), and only
#    THEN does the small "emerald" appear, as a static object already sitting
#    in the forest room (not the anthill entrance) — it's picked up by the
#    `take all` after shrinking back to normal size.
#  - Charging the energy crystal is a two-room puzzle: `put metal sceptre in
#    hole` at the tower roof, then `put energy crystal in metal holder` +
#    `press button` back in the dusty chamber below.
#  - The magical staff stays invisible until `use amulet of chaos on Zandor
#    the Sorcerer` is used inside Zandor's sanctum; only then is it visible
#    for `use energy crystal on magical staff` (removes its protection) and
#    `take`.
#  - Giving the magical staff to Madam Ingra triggers the endgame trap
#    (Zandor blasts the cottage, strips the inventory, arms a 2-turn death
#    countdown) — `break magical staff` must be the very next command.
look
southeast
northeast
up
pluck Giant Eagle
down
southwest
northwest
in
use feather on Zumble the cat
take dead mouse
out
southeast
south
east
east
east
in
give dead mouse to python
out
west
west
west
south
southeast
cross bridge
southeast
south
south
look at toad
north
north
northwest
cross bridge
northwest
north
east
east
east
in
talk to Yanda
2
out
west
west
west
north
northwest
in
talk to Madam Ingra
3
out
southeast
south
east
east
east
in
give bezoar to Yanda
out
west
west
west
south
southeast
cross bridge
southeast
south
south
use wart remover on toad
take toad
north
north
northwest
in
take shovel
out
cross bridge
northwest
north
dig roots
down
unlock chest
open chest
take ruby
take orange potion
drink orange potion
move chest
dig soft ground
take worm fruit
up
east
southeast
in
give ruby to Rumpel
take toy sail boat
out
northwest
west
west
use toy sail boat on lake
use boat
take dewberry
use boat
east
north
northeast
in
push stone eye
look at recess
take dragon's tooth
out
southwest
northwest
in
put toad in cauldron
put dewberry in cauldron
put dragon's tooth in cauldron
take red potion
drink red potion
out
southeast
south
west
use boat
look at ice pedestal
take sapphire
use boat
east
east
southeast
in
give sapphire to Rumpel
take metal rod
out
northwest
east
in
turn sign
in
move cabinet
open trapdoor
look at recess
take gold key
out
west
west
south
southeast
cross bridge
southeast
in
northeast
swing shovel
take egg
take copper key
southwest
out
south
south
southwest
in
northeast
north
north
northwest
cross bridge
northwest
north
east
east
east
in
talk to Yanda
2
out
west
west
west
north
northeast
up
talk to Giant Eagle
2
give egg to Giant Eagle
talk to Giant Eagle
2
in
look at table
take candle
x candlestick
unlock casket
open casket
take energy crystal
take fishing rod
down
north
down
pull leg
look at recess
take coronet
up
east
east
open cupboard
take pepper
west
look at oak table
take salmon
east
down
northwest
north
west
use fishing rod on mud
take metal sphere
use metal sphere on metal rod
east
north
northwest
cross bridge
northwest
north
east
east
use pepper on Eft the Fool
take eyeball
west
in
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
search barrel
take toy unicorn
out
tear toy unicorn
west
north
northwest
in
put worm fruit in cauldron
put eyeball in cauldron
put cotton wool in cauldron
take blue potion
drink blue potion
out
southeast
south
south
southeast
cross bridge
southeast
in
southeast
take spider web
northwest
out
northwest
cross bridge
northwest
north
north
northwest
in
put pepper in cauldron
put spider web in cauldron
put salmon in cauldron
take green potion
out
southeast
drink green potion
in
east
look
push gigantic emerald
west
push gigantic emerald
out
push gigantic emerald
west
take all
south
east
southeast
in
give emerald to Rumpel
take vial of chainrot
out
northwest
west
south
give coronet to Ghost Prince
take diamond
southeast
cross bridge
southeast
in
northeast
unlock cell door
open cell door
in
use vial of chainrot on chains
take amulet of chaos
out
southwest
out
northwest
cross bridge
northwest
north
north
northeast
up
talk to Giant Eagle
2
up
put metal sceptre in hole
down
in
put energy crystal in metal holder
press button
take energy crystal
down
use diamond on mirror
unlock metal door
open metal door
east
use amulet of chaos on Zandor the Sorcerer
use energy crystal on magical staff
take magical staff
west
north
east
east
down
northwest
north
north
northwest
cross bridge
northwest
north
north
northwest
in
give magical staff to Madam Ingra
break magical staff
