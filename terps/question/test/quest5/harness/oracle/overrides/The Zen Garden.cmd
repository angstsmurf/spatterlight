# The Zen Garden (Privateer, 2013, ASL 530) — WINNING script, derived purely
# from game.aslx (no published walkthrough exists; override-only wt="-" row).
# Win = `clap` in the Zen Garden while ALL FOUR dish-tableau states hold:
# woman holding parasol, old man drinking tea, frog held by heron, turtle
# visible on the rock. The win text never calls `finish` (Serpent's-Eye
# pattern), so the script ends with the game's own `quit`.
# Key beats / footguns:
# - 5 pottery fragments fuse into the dish when all carried (turnscript);
#   the dish lifts the fog and opens the west road to the river.
# - Heron must get EXACTLY 7 minnows (justfull) before `give frog to heron`;
#   6 or fewer = frog eaten (death), 8+ = toofull (death).
# - Bee (RandomChance(20), erkyrath seed 1234): arrives at the END of the
#   23rd turn spent in rose-bushes room 1 — 23 z's then `follow bee`.
# - `bow` to Mizuchi runs its beaten-flags AFTER a SYNCHRONOUS play sound;
#   QuestViva parks them on the wait slot, and this game has no later wait{},
#   so a SECOND `bow` (demon still visible) claims the slot and flushes the
#   first continuation. The second bow's own parked message resumes at
#   FinishGame — printed after `> quit` (faithful; native must match).
# - Endgame maze = stroke directions of the letters LNVZM (monk's paper /
#   inn screen "Follow the letters perfectly"): S,E N,SE,N SE,NE E,SW,E
#   N,SE,NE,S; wrong exit = ninja dart death in r15/r16. Give blossom to the
#   girl in the last chamber for the 7-fish haiku and a safe exit north.
# - Turtle turnscript cycle: 18 turns hidden / 5 visible, ticking from game
#   start; after the tea ceremony 19 z's land `clap` on the reappearance turn.
# - Load-time [error] errors=1 is the game's own duplicate `touch` verb
#   definition (dictionary key collision) — benign, breaker untouched.
ask woman about dish
out
take blossom
e
x pond
take colourful fragment
d
d
x beach
take pebble
x tracks
take large fragment
in
out
z
z
x boats
take basket
up
up
throw pebble
take frog
up
x dust
take bell
ne
e
n
x tavern
x door
take lantern
s
in
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
follow bee
look under beehive
take jagged fragment
out
w
sw
nw
in
x statue
take rice
out
se
ne
x water
drop rice
x markings
say typhoon
cross bridge
up
x ochre panel
up
take sword
down
n
n
out
cut bamboo
e
look behind tree
take bow
se
in
take winnowing fan
turn crank handle
turn crank handle
out
x stream
look under mosquitoes
take narrow fragment
nw
nw
w
give minnows to cat
nw
listen to cricket
x tuft
take helmet
nw
n
give helmet to warrior
take key
s
se
se
e
se
w
in
s
s
up
up
up
unlock door
open door
in
look under mat
take arrow
take blanket
wear blanket
out
down
down
down
n
n
out
e
nw
w
nw
w
sw
nw
in
x basin
x water
take fragment of pottery
look up
x fragment of pottery
fire arrow at thread
take brown fragment
x dish
w
x table
x sheet of paper
e
out
se
ne
e
se
e
se
w
in
s
s
down
cross bridge
sw
down
w
w
x river
cross ford
bow
bow
cross ford
n
tie bell
ring bell
take large coin
n
n
x fallen snow
take small coin
give large coin to vendor
give small coin to vendor
take parasol
in
x silk screen
look behind screen
open door
w
s
e
n
se
n
se
ne
e
sw
e
n
se
ne
s
give blossom to girl
n
out
s
s
e
e
e
in
give parasol to woman
give minnows to heron
give minnows to heron
give minnows to heron
give minnows to heron
give minnows to heron
give minnows to heron
give minnows to heron
give frog to heron
e
scoop tea
dip tea
stir tea
pour tea
serve tea
w
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
clap
quit
