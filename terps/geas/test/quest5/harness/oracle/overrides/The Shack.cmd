# The Shack (Mat Cooper / System Masters, 2020, Quest 5.8) — winning script
# RESULT: state=Finished, errors=2 (benign, see below) — full ending: the Void Cube
#   is dropped in The White Room, the "way out" sequence plays, credits, finish.
# SOURCE: none — no published walkthrough exists for this game anywhere in the
#   corpus. This script was derived from the game source (game.aslx inside the
#   .quest zip): the win condition is Ending 1, reached by dropping The Void Cube
#   in the White Room; the cube appears at the Shoreline statue once its three
#   "key parts" (tiara -> head, diamond ring -> finger, locket-on-chain -> neck)
#   are placed and the plinth block is examined.
# Solution chain, in brief:
#   - Shack: light (x crack, pull cord), toolbox (screwdriver + nails), small key
#     in flower pots behind curtain -> trunk -> wear farmhand clothes -> out.
#   - Woods: pocket knife under fence post; crab apple; drink stream -> brass
#     padlock key. Field: x stones/slabs/magpie, feed it the crab apple -> coin.
#   - Scarecrow: crumpled note (map to the tree) + red battery in pockets.
#   - Stables: unscrew bracket -> cultivator + screws. Farm house: small chain
#     (kitchen sink), mallet (pantry shelves), oil (behind tumble dryer; oils the
#     knife), heavy key (behind living-room photograph), ornate handle (master
#     bedroom tables), rubber gloves (en-suite cupboard, padlock key), blue
#     battery (bathroom toothbrush), torch + tiara (small bedroom), drawer fixed
#     with handle+screws+screwdriver -> coin opens metal box -> locket, chained.
#   - Millstone sharpens the oiled knife; silo: push hay bale, cut rope -> ladder
#     -> loppers. Boat shed (heavy key): oar + raft blueprints. Path 5: push
#     trailer -> jack. Up Stream: push barrels x3 (they float to Lake Edge).
#   - Cave (torch, both batteries): pick axe. Deep Woods pond: cultivator hooks
#     out the diamond ring. Lake Edge: gloves to pull cable -> crate; loppers cut
#     cable; pick axe smashes crate -> planks; nails+mallet; use blueprints ->
#     raft -> Shoreline statue -> place all three key parts -> x block -> cube.
#   - Tree hollow (crumpled note + torch): crawl to the Small steel door, jack it
#     open via the horizontal bar, glass tunnel -> White Room -> drop cube.
# Notes:
#  - "take brass key", not "take padlock key": the object's alias is "tiny brass
#    key", and Quest matches alias/alt only, so the source name is unmatchable.
#  - errors=2 is the game's own authoring bug, unavoidable on the mandatory
#    "open drawer": the drawer's onopen calls HelperOpenObject (isopen = true),
#    which re-fires the changed-isopen chain -> "Script execution depth exceeded
#    200" twice on stderr. Output is correct, the metal box is revealed, and the
#    error count stays far below the 20-error breaker (like Bony King's 31).
#  - The "Overall" timer (interval 410s, atmospheric flavour messages) is a
#    recurring timer, so the headless oracle leaves it dormant — deterministic.
#  - Red herrings not needed: cotton spool, corkscrew, rubber plug, safety razor,
#    shaving stick, xyzzy.
x crack
pull cord
x shelf
open toolbox
take screwdriver
take nails
pull curtain
x flower pots
take small key
unlock trunk
open trunk
take clothes
wear clothes
out
w
w
x fence
take pocket knife
w
x tree
take crab apple
n
drink stream
take brass key
s
e
e
e
s
s
x stones
x slabs
x magpie
use crab apple on magpie
take coin
e
x scarecrow
x jacket
open jacket
take crumpled note
x pockets
w
w
w
x bracket
use screwdriver on screws
take cultivator
take screws
w
knock on farm door
in
e
x sink
take small chain
e
x shelves
take mallet
w
w
w
x tumble dryer
x extractor
take oil
use oil on pocket knife
s
x fireplace
x mantlepiece
take photograph
take heavy key
n
e
up
s
x tables
take ornate handle
sw
unlock cupboard
open cupboard
take rubber gloves
wear rubber gloves
ne
n
se
x sink
open wash bag
open toothbrush
take blue battery
nw
w
x top bunk
pull duvet
take torch
use red battery on torch
use blue battery on torch
x bottom bunk
take tiara
e
e
x desk
use ornate handle on drawer
open drawer
use coin on metal box
take locket
use small chain on locket
w
down
out
e
e
use pocket knife on millstone
nw
x ledge
push hay bale
cut rope with pocket knife
up
x shelf
take loppers
down
se
e
s
use heavy key on boat shed
in
x rowboat
pull tarpaulin
take oar
x noticeboard
take blueprints
out
se
se
x trailer
push trailer
take jack
nw
nw
n
n
n
e
e
ne
nw
push barrels
se
push barrels
e
push barrels
n
e
e
use torch on cave
in
take pick axe
out
w
w
s
w
sw
w
w
w
w
w
x pond
use cultivator on glint
n
n
pull cable
cut cable with loppers
use pick axe on crate
x crate
use nails on planks
use blueprints
n
n
x statue
use tiara on statue
use diamond ring on statue
use locket on statue
x block
s
s
s
s
use torch on hollow
in
w
s
sw
w
x steel door
use jack on horizontal bar
s
s
s
drop cube
