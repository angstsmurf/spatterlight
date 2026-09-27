# Lost Cavern -- Simon Richards, 2020 (Quest 5.8, ASL 580) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 168 steps, MAX SCORE 100/100 ("You are a
# legend. ... Congratulations you have won the game!"). The win fires inside the
# trunk's addscript when game.score reaches >=100 on storing the 9th treasure.
#
# SOURCE: no published walkthrough exists anywhere; derived entirely from
# game.aslx. A large treasure hunt -- 9 treasures, each +5 on first take and +5
# on being stored in the camp trunk, plus puzzle sub-scores that make exactly 100.
#
# THE 9 TREASURES + how each is obtained:
#  * coin      -- Cliff by moss covered rocks: x rocks -> x hole reveals it down a
#                 gap; retrieve with a "stick with gum attached" (get coin with
#                 stick, +6 -- fishing bypasses the normal +5 take).
#  * Silver brooch -- Tree branch (west of river): throw the shiny object (tin foil,
#                 found by `x rushes` at Bullrushes) to lure the guardian blackbird
#                 away (+1), then take it.
#  * Sword     -- Scrub (west of river): `dig` with the shovel (beach by pool) digs
#                 up a Box; open box (answer 1 to disambiguate vs the matchbox), take.
#  * Golden crown + StoneDisk -- Inside temple, reached across the swamp maze
#                 (Edge of swamp: s,e,s,w,n,n). The disk is the key for the cavern.
#  * Emerald   -- Stalagmite room: burn the cobweb with matches (+1) to open the
#                 huge cavern, then drop the High-ledge boulder on the guardian
#                 spider with the crowbar (+1) to open the Stalagmite room.
#  * Orb       -- beehive room: `hit hive` to enrage the bees, flee east so they
#                 chase you, then east again into the Flower crater where the giant
#                 flowers distract them (+1); return and take the orb.
#  * silver bowl -- DarkCorner, past the narrow crack (which requires an EMPTY
#                 inventory -- `drop all`, squeeze `e`, `take all` on return). The
#                 bowl cannot be carried back through the crack, so it is floated
#                 out: take a basket, put bowl in basket (-5), put basket in river
#                 (+1) -> it washes to the surface Pool at waterfall bottom, where
#                 it is retrieved (+5).
#  * GoldenStatue -- temple altar, reached by solving the three alcove dials
#                 (West=5, East=7, North=2 -- the mural animal counts; the last dial
#                 set trips the mechanism, +1, unlocking the Stone Steps down).
#  * Death mask -- Tomb: `insert stone disk` in the altar (+1) opens the door east.
#
#  * The pack of wild dogs blocking the narrow passage is killed by dropping the
#    "poisoned meat" (mix the Forest-clearing mushroom into the River-bend meat, +1).
#
# STRATEGY: gather all surface/swamp items and treasures first; DEPOSIT the four
# surface treasures (coin, brooch, sword, crown) in the trunk before entering the
# caverns, so the narrow-crack `drop all` only drops tools (no DecreaseScore churn).
# Then collect the five cavern treasures and deposit them; the death-mask deposit
# crosses 100 and wins.
#
# FOOTGUNS (all verified against game.aslx / the QuestViva oracle):
#  * The trunk starts CLOSED -- `open trunk` before the first deposit.
#  * The gum is hidden on the sleeping bag -- `x sleeping bag` reveals it.
#  * `open box` at Scrub is ambiguous with the matchbox -- answer `1`.
#  * The narrow crack (By narrow crack <-> Underground river) hard-requires an empty
#    inventory in BOTH directions (ScopeInventory()=0).
#  * putting the bowl in the basket is a real -5; it is recovered at the Pool.
s
x sleeping bag
take gum
chew gum
n
sw
take stick
put gum on stick
s
in
take crowbar
out
nw
x stump
take mushroom
sw
x rushes
take shiny object
n
ne
ne
x rocks
x hole
get coin with stick
sw
w
w
n
take shovel
s
s
up
throw shiny object
take brooch
down
s
x carcus
take meat
mix mushroom with meat
n
w
dig
open box
1
take sword
ne
e
se
s
s
s
e
s
w
n
n
take crown
take stone disk
out
w
n
n
ne
e
s
open trunk
put coin in trunk
put brooch in trunk
put sword in trunk
put crown in trunk
score
n
w
nw
in
n
drop poisoned meat
n
nw
nw
w
w
w
se
e
drop all
e
take baskets
n
nw
take bowl
put bowl in basket
se
s
put basket in river
w
take all
score
w
nw
hit hive
e
e
w
w
take orb
e
e
n
n
w
turn dial to 5
e
e
turn dial to 7
w
n
turn dial to 2
s
down
n
n
take golden statue
insert stone disk
e
take death mask
w
s
s
up
out
s
e
se
se
sw
burn cobweb with matches
w
up
up
up
up
move boulder with crowbar
down
down
down
down
w
take emerald
e
e
ne
s
s
out
score
w
take bowl
e
se
e
s
put emerald in trunk
put orb in trunk
put silver bowl in trunk
put golden statue in trunk
put death mask in trunk
score
