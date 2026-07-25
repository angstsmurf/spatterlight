# Gilleholmen (Joachim Parrow, April 2026, ASL 580) -- 51 rooms, 571 objects.
# A brand-new release with no published walkthrough; this 302-step script was
# derived from the game source and reaches the game's ONLY win: carry the
# BURNING torch down into the cave below the gate and answer the guardian's
# closing question with anything containing the word "remember".
#
# The game is an autobiographical Swedish-summer-house memory piece. There is
# no score command; instead the author keeps a hidden counter,
# player.progresspoints (starts at 1, player.maxprogresspoints = 42), bumped by
# a `progress` call at each of the ~41 distinct milestones. `progress` only
# NARRATES ("Progress increased to N") when game.authorised is set, which is
# what the debug command `auth Leviathan` does (the password is sitting in
# plain sight in <start>, alongside teleport / summon / carries and friends).
# This script deliberately does NOT authorise -- but run with `auth Leviathan`
# prefixed it prints exactly 42 increases and ends on "Progress increased to
# 43", i.e. it is a MAX route: every non-duplicate progress site in the game
# fires, including the ones that are pure flavour and cost nothing.
#
# There is exactly one `finish`, in `win`. Nothing in the game can kill you.
#
# The spine of the game, all verified against the source:
#  - The house is LOCKED and there is no key. The only thing that opens it is
#    `flykite` (line 15905): fly the mended kite from the balcony parapet and
#    the meal gong ends the reverie, dropping you in the Dining room and
#    UnlockExit(outsidetolivingroom). Everything indoors is gated behind it.
#    Building the kite is the whole first act: the tangle is in the trash can
#    north of the house, `look under table` on the terrace yields the paper to
#    mend it with, and the fishing line comes from the boat at the end of the
#    pier -- which is only reachable after `cut weeds with weed cutter` (x3)
#    clears the stair to the beach. `fly kite` then eats TWO `get input`
#    answers before the wind takes the kite; any text will do.
#  - `sit on bench` in the sauna is load-bearing, not flavour. The sauna song
#    ("Inga said you are so dumb") is the ONLY clue to the kids' guessing-game
#    riddle at the top of the stairs -- "Who is Lars' wife?" -- and the answer
#    `inga` (line 7826) is what buys entry to the whole upper floor.
#  - The towel is won by throwing stones at the South point: three throws at
#    the tree trunks to build up South point.treethrows, and only then does
#    `throw stone at white cloth` knock it down. Carrying the towel to the end
#    of the pier enables `swim`, which drops everything you carry, teleports
#    you into the Living room, opens the pantry and unlocks kitchentopantry.
#  - The house has no power. The fuse box only becomes reachable from the top
#    of the foldable ladder in the Hall, and ONLY while the office door is
#    CLOSED (climb2, line 4515) -- so the route closes it, climbs, takes the
#    burnt fuse, `read spent fuse` for its label (AXLCD10A), then finds the box
#    of spares by `look under desk` in the office, asks for that exact label,
#    and climbs back up to fit it. That is what lights the Cabin downstairs.
#  - The garage is opened with the "garage key", which parses ONLY as `key`
#    (its alias); it hangs inside the breakfast-room cupboards and is revealed
#    by `wash dishes` in the kitchen, which opens them. Once inside the garage
#    you must `close door` and `lock door with key` behind you: leaving the
#    hallway while the door is locked runs `checkgarage` (line 16023), the kids
#    are shut out, and that is a milestone. Left unlocked they rush in and
#    dump you back outside.
#  - The lens (a bottle bottom) is the game's only fire-starter. `x machine`
#    in the brewery reveals the crank; three turns produce a small bottle, then
#    feeding it the big bottle from the storage-room straw heaps and turning
#    twice more wrecks the machine, spilling the lens and a floor of glass
#    shards. The shards make the lens unreachable until you `sweep` them with
#    the broom -- so sweep first, then take the lens. Because the turnscript
#    (line 6579) forbids walking while the lens is held bare, it has to ride in
#    the bucket.
#  - `x mound` at the garage's upper-floor window is what MakeObjectVisible's
#    the path up the mound; the two direct exits from West of mound are locked
#    forever.
#  - The crack in the mound's roof is only visible from ABOVE after you have
#    seen it from BELOW: Cavern.beforefirstenter reveals it. The cavern is
#    reached through the Cabin's trap door, which will not open while the ants
#    are alive. Killing them takes the pesticide in the large can, which sits
#    on the pantry's top shelf.
#  - Getting the can down is the game's best joke. The ladder cannot be carried
#    up to the top shelf's room and back with anything in hand, and `climb
#    ladder` refuses if you carry more than 20 volume; so you climb, take the
#    can, DROP it on the top of the ladder, climb down, and `fold ladder` --
#    which tips it off and rattles it down to the floor (line 4483). The ladder
#    itself only gets to the pantry via the cart: `pull ladder to out`, `take
#    ladder`, `put ladder in cart` (loading it is the milestone), pull the cart
#    round to the kitchen door, then `pull ladder to in` twice.
#  - Endgame: `widen crack with broom` on top of the mound (the broom's own
#    `use broom on crack` also widens it but scores nothing), then back down
#    into the cavern to `put torch in hole`, back up to `pull torch` free, then
#    `burn torch with lens` -- which works ONLY in the Sand pit, the one sunlit
#    room, with the lens held directly in hand. Then west, west, down, and the
#    reply.
#
# Inventory model to keep in mind while reading the route: player.maxvolume is
# 100 and CheckLimits only fails ABOVE it, so broom 70 + bucket 25 + lens 5 is
# legal to the gram, as is torch 70 + bucket 25 + lens 5. `pull cart`, `pull
# torch` and the crawl through the trap door all require carriesmuch() to be
# false, i.e. genuinely empty hands, hence the drop/retrieve shuffles.
#
# No RNG, no combat, no timers on this path (Quest timers are real-time and
# never fire headless anyway, so the forest timer is moot). errors=0.
in
east
northeast
east
north
open trash can
take tangle
east
put kite on table
look under table
north
north
in
look under carpet
take moccasins
out
sit on swing
north
north
in
sit on bench
out
south
west
west
northeast
west
south
take spade
south
east
north
east
down
down
south
dig
drop spade
enter tub
north
north
climb rock
south
out
out
enter boat
take oars
take fishing line
out
in
in
up
up
south
look under roses
take weed cutter
east
down
south
cut weeds with weed cutter
cut weeds with weed cutter
cut weeds with weed cutter
drop weed cutter
north
up
take paper
mend kite with paper
tie fishing line to kite
take kite
down
south
put kite on parapet
climb parapet
take kite
fly kite
i must let it go
let it go
west
west
wash dishes
east
south
west
up
inga
north
lie on bed
take book
south
put book on table
east
north
look under chairs
take towel
south
west
wrap towel around book
take bundle
east
out
throw bundle over railing
in
west
down
east
out
south
south
throw stone at trunks
throw stone at trunks
throw stone at trunks
throw stone at white cloth
take towel
north
east
down
down
out
out
swim
west
open office door
west
close office door
unfold ladder
climb ladder
open fuse box
take spent fuse
read spent fuse
climb down
fold ladder
open office door
east
x desk
look under desk
open box
take fuses
AXLCD10A
west
close office door
unfold ladder
climb ladder
put fuse in fuse box
climb down
fold ladder
drop spent fuse
open office door
east
east
north
west
take key
east
south
out
down
down
south
take bucket
north
up
up
in
west
west
take broom
out
southwest
south
unlock door with key
in
close door
lock door with key
east
x heaps
drop broom
drop bucket
take big bottle
west
west
x machine
turn crank
turn crank
turn crank
put big bottle in machine
turn crank
turn crank
east
east
take broom
take bucket
west
west
sweep
take lens
put lens in bucket
east
up
x mound
down
unlock door with key
out
north
east
drop broom
drop bucket
north
north
pull cart to south
pull cart to west
in
pull ladder to out
take ladder
put ladder in cart
pull cart to north
west
in
east
east
north
west
west
out
pull ladder to in
pull ladder to in
unfold ladder
climb ladder
take large can
drop large can
climb down
fold ladder
take large can
out
east
east
south
west
down
open can
pour pesticide on trap door
up
east
out
south
west
take broom
take bucket
in
east
down
sweep
open trap door
drop broom
drop bucket
down
up
down
up
take broom
take bucket
up
east
out
south
west
southwest
north
widen crack with broom
drop broom
drop bucket
south
east
in
east
down
down
up
take torch
put torch in hole
down
up
up
east
out
south
west
southwest
north
pull torch
take bucket
south
west
northeast
north
take lens
burn torch with lens
put lens in bucket
south
west
west
down
I remember
