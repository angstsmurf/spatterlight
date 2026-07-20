# Lost in the Shadows of Time (K.A. Williams, 2013, ASL 540) — derived winning
# walkthrough. Self-described difficulty "Hard", cruelty "Nasty".
#
# No published walkthrough exists anywhere and the game ships no <walkthrough>
# steps, so this 61-step script was derived entirely from the game source.
# wt="-" in corpus.tsv.
#
# THE ENDING IS A DEATH. You are a road-accident victim whose soul has been
# spliced into a Wood Elf; the only way home is `attack nezrai` in the Castle of
# Shadows, which gets you stabbed and wakes you from the coma in a hospital bed
# (game.aslx:1783). `tell nezrai about world` is the other branch and it dead-ends
# on "You can ATTACK NEZRAI. It seems your only option." The win then needs one
# more turn — `ask nurse about who` (game.aslx:1932) — whose nested waits print
# the credits and call `finish`. Stopping at the hospital room is NOT the win.
#
# EXITS RESOLVE BY ALIAS, NOT BY INHERITED DIRECTION. Most exits are plain
# compass words, but six carry a custom alias while still inheriting a direction
# (`northdirection` etc.). The inherit only fills the map slot — Core's `go`
# matches the alias — so at those six a bare compass word is "You can't go there"
# even though the room banner shows a direction. They must be spelled out:
# `go gate of hell`, `go through the gate of loss to the bog of whispers`,
# `go fly`, `go secret cave entrance`, `go violet`, `go to secret tunnel`.
#
# THE ITEM CHAIN is strictly ordered, and two steps are one-way:
#   glass bottle (Forest1) -> lets you `take venom` from the dead Giant Spider
#   ask troll about whiskey -> the EMPTY Whiskey Flask. This must be collected
#     BEFORE flying east, because the Peddler sells whiskey only to someone
#     already holding the flask, and the Hellish Plains exit is one-way (the
#     Secret Cave Entrance drops you back at the Dead Forest cave).
#   Lavender wants venom THEN the pixie, in that order (she says so) -> Hellish Key
#   Hellish Key -> Gate of Hell -> Barren Wasteland
#   Spider Web (Dead Forest) traded to Na'shan -> White Flute + Gold Coin
#   play flute at the ravine -> unlocks the Hell Bird ride
#   flask + gold coin -> the Peddler's Whiskey -> the troll -> Slimey Key
#   Slimey Key -> Gate of Loss -> the Bog of Whispers
#   Skull Key (Heartstring Cave) -> the Gate of Doom -> Castle of Shadows
#
# THE BOG OF WHISPERS is a light maze where all but one exit per node is an
# instant `finish` (turtle, mosquito, quicksand, carnivorous plant, rat). The
# survivable path is southwest, southwest, south, southeast, then `go violet`.
# "Violet" is the answer to the Slimey Sign's riddle — "WHAT COLOR ARE MY EYES?
# --LAVENDER" — and Lavender's description (game.aslx:1111) gives her violet
# eyes. Its three siblings there are Blue (back a node), Purple and Lavender
# Purple (both fatal).
#
# THE SKULL DOOR wants the next triangular number: the sign reads "ETERNITY IS A
# TRIANGLE: 3 6 10 ...", so `tell skull about 15`. Heartstring Cave is dark and
# needs the Adventurer's Torch — `ask adventurer about cave` then `light torch`
# (its light script calls SetLight on the cave).
#
# DEATHS AVOIDED: the small opening past the spider in the first cave (instant
# finish), attacking or insulting Na'shan, and every wrong light in the bog.
#
# A `CONSTANT` turnscript prints "What are you going to do?" after every turn —
# authored noise, not an error. errors=0, no RNG on the path.
west
take bottle
west
ask troll about whiskey
east
east
north
take spider web
in
take venom
out
south
south
in
take pixie
out
north
# --- Lavender: venom THEN pixie -> Hellish Key ---
east
east
give venom to lavender
give pixie to lavender
# --- Gates of Hell -> Barren Wasteland ---
west
west
north
north
use hellish key on gate
go gate of hell
# --- Na'shan: trade the spider silk for the flute and gold ---
ask man about help
east
play flute
go fly
# --- Peddler: buy the whiskey, then drop back through the secret cave ---
ask peddler about whiskey
go secret cave entrance
out
south
west
west
# --- Troll: whiskey -> Slimey Key -> the Gate of Loss ---
give whiskey to troll
use slimey key on gate of loss
go through the gate of loss to the bog of whispers
# --- Bog of Whispers: every other light is an instant death ---
southwest
southwest
south
southeast
go violet
# --- Plains of Blood: torch, skull riddle, Skull Key ---
ask adventurer about cave
light torch
in
tell skull about 15
north
take skull key
go to secret tunnel
out
# --- Back east to the Gates of Doom ---
south
east
east
east
use skull key on gate
east
# --- Castle of Shadows: dying here is the only way home ---
attack nezrai
ask nurse about who
