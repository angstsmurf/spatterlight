# Osterhase — by XanMag, 2018 (ASL 550, standard parser style, not gamebook).
# No published walkthrough exists — derived from game source (game.aslx). A
# 25-room Easter-egg hunt: 25 room objects plus 7 sub-locations (Storage,
# tree1, underwater lake, sunken ship, hatchery, exit room, space room). The
# win/ending is entering the "exit room"'s magic hole (`down`) and confirming
# `yes`, which prints a ranking keyed to basket.ContentCount (0-25) and calls
# `finish`.
#
# AUTHOR BUG (confirmed via exhaustive source grep, capping the max score at
# 20/25): 5 of the 25 eggs are permanently unreachable. The dog-fetch
# minigame in room5 ("Fido's field") — the sole source of the golf ball
# retriever, and hence the mossy egg — needs the "stick" object, which is
# parented under "Storage". Storage's only entrance is the "thicketlock"
# exit, and `grep -n 'UnlockExit'` shows exactly one call in the whole
# codebase (for "climbtreelock", triggered by dropping the black cat in
# room9) — "thicketlock" is never unlocked anywhere. `take stick` while
# standing in room5 (where the stick's own throw-script checks
# `game.pov.parent = room5`) returns "I can't see that." Cascade: golf ball
# retriever (needs stick, room14... no, room5) -> orange egg (needs the
# retriever, room19) -> cool blue egg (needs the orange egg to melt an icy
# block, room4) -> clear egg (needs the cool blue egg to freeze a fountain,
# room11) -> floral egg (needs the clear egg to grow a plant, room10). This
# script does not attempt that dead branch; final score is 20/25, ranking
# "EASTER BUNNY'S BUDDY!" — the correct, provably-maximal outcome.
#
# Slot machine (room14, "gambler's field"): `use slot machine` is a two-stage
# RNG state machine (RandomChance(40) for a stick, then RandomChance(85) for
# a golden egg) gated on carrying the bag of coins. The oracle's RNG is
# deterministic per fixed script — empirically the stick lands on the 3rd
# use and the golden egg on the 7th; this script issues exactly 7 uses.
#
# Corpus-wide footgun (same one documented for other games in this
# overrides/): objects inside a `hidechildren` container (the tree1 "nest",
# the room23 "altar") are NOT directly takeable by full name until the
# container has been examined at least once — `x nest`/`x altar` first,
# then `take blue egg`/`take horned egg` resolves. Two more one-off name
# quirks found by trial: the "shell pile" object (room22) only resolves via
# its `<alias>`, "search pile of worthless shells" (the primary name fails);
# the sunken-ship "black egg" only resolves via its `<alias>` "pirate egg"
# (the primary name fails even though the sibling "bag of coins" takes fine
# by its primary name).
#
# Route: boring egg (room2, search tuft of grass) -> room9 blue egg is
# snatched by a vulture into the tree1 nest on the first attempt (feather is
# left behind) -> tickle big guy (room3) with that feather -> tell the
# skinny girl (room20) about big guy, then tell big guy about her -> reveals
# black hair -> black cat -> drop the cat back in room9 to unlock the tree
# -> climb the tree, examine the nest, take the now-recoverable blue egg ->
# skinny girl's own pink egg (`speak skinny girl`) -> glasses (exit room) ->
# wearing them lets the Magician's cup game auto-award the glowing egg
# (`speak Magician`) -> rock egg (room6, search rock pile) -> piano key
# (room16, play piano then take it) -> lame egg (room8, alias "lame egg") ->
# all 3 Easter Riddler riddles (room25: b/Osterhase, d/killed his cat,
# a/HydroHare) -> underwater lake: first `in` triggers a scripted (not RNG)
# barracuda attack that bounces you back to room24 and reveals the orbs;
# swim back down, take the orbs, `in` again succeeds -> sunken ship: piano
# key opens the chest directly via its own selfuseon handler (bypassing the
# lock/key mechanism entirely) -> pirate egg + bag of coins -> slot machine
# (room14, stick then golden egg) -> cream egg (room15) -> yellow egg
# (room17) -> xyzzy (room18) teleports to the space room for the shiny egg
# -> welcome egg (room21, lift welcome mat, one-step reveal+take) -> pet
# mother goose (hatchery) for the feathery egg -> cracked egg (room22,
# search the shell pile) -> give the boring egg to the Giant Minotaur
# (room23) to put him to sleep, then take the horned egg -> smelly egg
# (room12, needs a second visit for the turds to appear, then search+take)
# -> exit room, down, yes. Final score 20/25. errors=0.
east
search tuft of grass
take boring egg
southeast
southeast
south
south
southwest
southwest
west
take blue egg
take feather
east
northeast
northeast
north
north
northwest
tickle big guy with feather
southeast
south
northwest
southwest
northwest
tell skinny girl about big guy
southeast
northeast
southeast
north
northwest
tell big guy about skinny girl
look at black hair
take black cat
southeast
south
south
southwest
southwest
west
drop black cat
up
x nest
take blue egg
down
east
northeast
northeast
north
northwest
southwest
northwest
speak skinny girl
southwest
north
take glasses
wear glasses
south
northeast
southeast
northeast
southeast
south
southwest
speak Magician
northeast
search rock pile
north
north
northwest
northwest
southwest
northwest
play piano
take piano key
southwest
southwest
south
south
southeast
southeast
east
east
take lame egg
northeast
northwest
speak Easter Riddler
b
speak Easter Riddler
d
speak Easter Riddler
a
southwest
down
in
down
take orbs
in
use piano key on chest
take pirate egg
take bag of coins
out
up
northwest
southwest
northwest
north
north
use slot machine
use slot machine
use slot machine
use slot machine
use slot machine
use slot machine
use slot machine
northeast
take cream egg
northeast
southeast
take yellow egg
northwest
southwest
southwest
south
northeast
xyzzy
take shiny egg
southeast
northeast
southeast
lift welcome mat
in
pet mother goose
out
northeast
search pile of worthless shells
take cracked egg
southeast
south
southwest
northwest
southwest
northwest
give boring egg to Giant Minotaur
x altar
take horned egg
southwest
northwest
southeast
northwest
search turds
take smelly egg
north
northeast
southeast
north
look at basket
down
yes
