# ZORK1_beta (KV's Quest Viva port of Zork I, v0.8): all 350 points and the Stone Barrow ending (state=Finished).
# The usual route: egg, troll, Loud Room bar, sluice gates, exorcism, coffin, boat to the rainbow (pot, scarab, buoy),
# Atlantis/mirror, the coal mine with the torch lowered in the basket and the diamond machine, then the egg given to
# the thief so his death leaves it open, and the canary wound on the Forest Path for the bauble.
# The game has its own Z-machine RANDOM seeded from the clock, and its state decays to a constant within ~30 calls
# (after which every PROB succeeds). `assert:RANDOM(-N) = 0` reseeds it, as `#random N` would; it is done before
# each roll that matters, seeds found by search: troll 12, both dark trips through the Drafty Room 158 and 2,
# thief 12. Seven items is the carry limit, so things are dropped or cased as soon as they are spent.
# Port quirks the route walks around: the boat's landing check reads the PREVIOUS walk's direction, so the Dam Base
# is reached by `e`; a light only counts when carried, so the torch in the basket does not light the Drafty Room and
# both trips there are dark (the passage only admits items of size 4 or less, so the lamp cannot come); candles left
# burning keep warning, so they are blown out before being dropped.
assert:RANDOM(-1) = 0
n
n
u
take egg
d
s
e
open window
w
take sack
w
take lamp
take sword
turn on lamp
e
u
take rope
d
w
move rug
open trap door
d
n
assert:RANDOM(-12) = 0
kill troll with sword
kill troll with sword
w
s
e
u
take coins
sw
e
s
se
ulysses
e
e
open trap door
open case
put coins in case
drop sack
drop egg
d
s
e
take painting
w
n
u
put painting in case
drop sword
d
n
e
e
e
echo
take bar
u
e
n
take matches
n
take wrench
push yellow
s
s
turn bolt with wrench
drop wrench
s
sw
s
se
e
tie rope to railing
d
take torch
turn off lamp
s
take bell
s
take book
take candles
d
d
ring bell
take candles
light match
light candles with match
read book
s
take skull
n
u
n
n
n
w
w
s
u
put bar in case
put skull in case
drop book
blow out candles
drop candles
drop matches
d
n
e
e
se
e
d
s
e
take coffin
w
s
pray
e
s
e
w
w
open coffin
take sceptre
put coffin in case
d
n
e
e
n
ne
nw
n
take trunk
n
take pump
n
take trident
s
s
s
e
e
inflate plastic with pump
drop pump
put sceptre in boat
board boat
take sceptre
launch
z
z
z
z
take buoy
e
disembark
take shovel
ne
dig sand with shovel
dig sand with shovel
dig sand with shovel
dig sand with shovel
drop shovel
take scarab
sw
s
s
wave sceptre
w
w
take pot
sw
u
u
nw
w
w
w
put trunk in case
put trident in case
put scarab in case
put pot in case
put sceptre in case
open buoy
take emerald
put emerald in case
drop buoy
open sack
take garlic
take matches
d
n
e
e
n
ne
e
n
n
take screwdriver
s
s
s
sw
s
s
s
rub mirror
n
w
n
w
n
take jade
e
turn on lamp
put torch in basket
n
d
take bracelet
e
ne
se
sw
d
d
s
take coal
n
u
u
n
e
s
n
u
s
put coal in basket
put screwdriver in basket
lower basket
n
d
e
ne
se
sw
d
d
w
drop lamp
drop jade
drop bracelet
assert:RANDOM(-158) = 0
w
take torch
take coal
take screwdriver
s
open lid
put coal in machine
close lid
turn switch with screwdriver
open lid
take diamond
n
put diamond in basket
put torch in basket
put screwdriver in basket
assert:RANDOM(-2) = 0
e
take lamp
take jade
take bracelet
e
u
u
n
e
s
n
u
s
raise basket
take diamond
take torch
w
s
e
s
d
u
put diamond in case
put jade in case
put bracelet in case
turn off lamp
take sword
take egg
w
w
u
give egg to thief
assert:RANDOM(-12) = 0
kill thief with sword
kill thief with sword
take chalice
take egg
take canary
d
e
e
put chalice in case
put egg in case
drop sword
e
e
n
n
wind canary
take bauble
s
e
w
w
put canary in case
put bauble in case
put torch in case
score
e
e
s
w
sw
w
quit
