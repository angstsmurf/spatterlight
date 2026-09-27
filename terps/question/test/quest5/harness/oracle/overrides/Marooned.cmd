# Marooned (subtitle: Two Men and a cat) -- Simon Mellor -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 188 steps. Ends on bridge2's `enter` script
# ("The End.." + DisplayHttpLink + `finish`) after the sub2 generator-room fuse
# pull and the Svetlana reunion at the gorge bridge.
#
# SOURCE: no published walkthrough exists -- the script was derived entirely
# from game.aslx. You're Gus, crash-landed on an alien desert planet; you pick
# up a marooned 1985 text-adventure character (Ed Lines, from a fictional game
# "Subsunk") and a stray cat (Christa) along the way. Structure: desert
# fetch-quests -> blow up a boulder with home-made gunpowder -> craft a torch
# to light a tunnel -> get lost in a jungle on purpose (11 turns) until the
# Communicator's own hint kicks in -> rescue the cat from a bird with a mirror
# -> get a Hydrogen canister from its nest -> build 3 balloons and fly a rope
# across a gorge -> a floating rope opens a hole to Ed's sub -> pull the fuse
# -> reunite with Svetlana's people across the bridge.
#
# ALIAS-VS-INTERNAL-NAME TRAPS: several objects are misspelled internally but
# have a correctly-spelled <alias> the parser actually needs:
#  * internal "potasium nitrate" (alias "Small pile of Potassium Nitrate") --
#    `take potassium nitrate`, NOT `take potasium nitrate`.
#  * internal "Sulpher" (alias "Small pile of Sulphur") -- `take sulphur`.
#
# WEIGHT VS OBJECT-COUNT CAPS: `maxobjects`=8 is COSMETIC -- "You can't carry
# any more" still lets the pickup succeed. `maxvolume`=12 is the REAL cap --
# "You're carrying too much weight, drop something.." means the pickup FAILED
# (no "You pick it up." follows). The Core drill (volume 10) needs everything
# droppable dumped first (piece of metal / magnifying glass / potassium
# nitrate, each volume 1) so only the Compass + Water Bottle (2) are carried.
#
# WATER-BOTTLE GATING: two things check `Got(Full water bottle)` and refuse
# without it -- the "Looking out across an expance of sand" room (bounces you
# back south) and the Compas 2 (compass) `use` script (refuses to travel at
# all). `fill water bottle` accepts either "Pool of water" at the original
# shaded area OR the differently-named "Pool of water 2" (alias also "Pool of
# water") in the Small Alcove of the Northern desert cluster -- convenient
# since you pass through there anyway. Each compass trip CONSUMES the full
# bottle on arrival, so every leg needs its own refill beforehand.
#
# "IN"/"OUT" QUIRK: entering "Inside your escape pod" the first time triggers
# a cutscene whose own wait-block auto-MoveObjects the player straight back to
# "Your Escape Pod" within that same turn -- a follow-up `out` fails with
# "You can't go there," so it's omitted here.
#
# DOWSING GATE: `use pair of wooden sticks` (get input matching water/devine/
# hold) only opens the dig spot when used while standing in the shaded area --
# elsewhere it's a no-op. That's what reveals the Pool of water there and lets
# `use shovel` uncover the north shortcut back to the middle of the desert.
#
# GUNPOWDER: Charcoal (4-visit campfire event at the Small Alcove), Sulphur
# (the pool-of-tar room) and Potassium Nitrate (desert) all `use ... on
# bucket`; a turnscript auto-swaps the Bucket to "Bucket full of gunpowder"
# once all three are in. `use bucket on hole` pours it into the drilled
# boulder, `use fuse on hole` arms it (fuse = `use piece of string on pool of
# tar`), and `use magnifying glass on huge boulder` detonates it.
#
# JUNGLE MAZE: entering "Entrance to a dense jungle" sets a flag that a global
# turnscript watches every turn thereafter; at turn 11 it silently arms the
# Communicator. `use communicator` at that point (from ANYWHERE in the maze --
# its script always teleports you back to the entrance first) prints the
# actual solution and creates a jungle8->Clearing shortcut exit; the 10-step
# route it prints (`north west north north east east north north west north`)
# was verified byte-for-byte against the room graph. 11 `wait`s here stand in
# for genuine wandering.
#
# NEST/BIRD: the very-large-nest's Hydrogen canister is gated on Christa being
# "found" again, which needs the bird scared off FIRST -- `use polish on piece
# of metal` crafts a Mirror (both already carried from earlier), then `use
# mirror on bird`, THEN `look at very large nest` yields the canister.
#
# DESCRIPTIVE EXIT ALIAS: Clearing's return exit to the jungle entrance has
# alias "Back the way you came" -- a bare `back the way you came` is "I don't
# understand"; it needs the `go` prefix: `go back the way you came`.
#
climb ladder
down
northwest
take atmosphere sampler
use atmosphere sampler
take a long branch
break a long branch
southeast
remove spacesuit
drop spacesuit
west
take shovel
west
take water bottle
north
take piece of metal
open packet of condoms
take a red condom
take a blue condom
take a green condom
west
use pair of wooden sticks
water
use shovel
fill water bottle
look
drop broken atmosphere sampler
drop shovel
east
south
east
east
east
east
take magnifying glass
west
west
north
north
take potassium nitrate
north
west
south
in
use compass
east
drop piece of metal
drop magnifying glass
drop potassium nitrate
take core drill
west
west
west
use core drill on huge boulder
drop core drill
east
east
east
take piece of metal
take magnifying glass
take potassium nitrate
take bucket
take hacksaw
south
take note
look at note
use keypad
16061963
take polish
take piece of string
take communicator
out
east
take sulphur
use piece of string on pool of tar
west
west
west
west
east
look at remains of a fire
take charcoal
east
east
use charcoal on bucket
use sulphur on bucket
use potassium nitrate on bucket
look
west
west
west
use bucket on hole
use fuse on hole
use magnifying glass on huge boulder
look
west
west
fill water bottle
west
north
take baseball bat
south
east
east
use compass
use hacksaw on spacesuit
west
west
north
west
fill water bottle
east
south
east
east
use compass
use cloth on baseball bat
east
east
use torch on pool of tar
use magnifying glass on tar covered torch
west
west
west
west
north
look
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
use communicator
north
west
north
north
east
east
north
north
west
north
west
play drum kit
east
northeast
climb tree
use polish on piece of metal
use mirror on bird
look at very large nest
down
southwest
north
use hydrogen canister on red condom
use hydrogen canister on blue condom
use hydrogen canister on green condom
use red balloon on rope
use blue balloon on rope
use green balloon on rope
push floating rope
south
go back the way you came
take flaming torch
east
fill water bottle
east
use compass
west
west
north
west
fill water bottle
east
south
east
east
north
north
north
down
use intercom
