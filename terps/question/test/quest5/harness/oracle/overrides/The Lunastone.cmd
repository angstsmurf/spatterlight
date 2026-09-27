# The Lunastone (Craig Dutton, 2014, ASL 550) - winning walkthrough, best ending.
# Derived from source; no published walkthrough exists.
#
# Seven islands on the ocean planet Aquana, reached ONLY by typing sector
# co-ordinates into the flying craft (`use flying craft`, then X, then Y). Every
# co-ordinate, the tower password (a constellation) and the plant needed for the
# Cave of Dreams potion are randomised at game start by pickstar/randoms/
# plantpicked, so the literal numbers below are the ones the deterministic
# oracle RNG (ErkyrathRandom, seed 1234) produces:
#
#   Neos     146, 146   (metal plate, cleaned with the oiled cloth)
#   Chanra   246, 246   (leaflet, small print on the back)
#   S Chanra 246, 247   (the arboretum plaque: "south side" = Sector Y plus 1)
#   Scython  346, 346   (the painting frame in the hut)
#   Palamore 446, 446   (written under the snowglobe)
#   Q'rel    646, 646   (X from the Cave of Dreams vision, Y from the ocean map)
#   Envahr  1646, 1646  (Krelos's message, once the Lunastone is taken)
#   keypad  = X + Y of Q'rel = 1292;  constellation = "mountain"
#   potion plant = Serpicous Rose (Neos garden)
#
# FOOTGUNS
#  * `say <wrong constellation>` outside the tower is INSTANT DEATH ("lockdown
#    ... your adventure is over") - the game's only unwinnable-by-one-command.
#  * The player has maxobjects 6 (feature_limitinventory), so the route below
#    stashes the leaf, ivy and snowglobe in the Palamore throne room and drops
#    each key/map/bowl the moment it has been used.
#  * Every surface/container here has `hidechildren`: `look at shelf` / `look at
#    table` / `look at recess` first, or its contents are not in scope at all.
#  * The repository lever must be left on image 3 (Heat). The `hotpipes`
#    turnscript switches the turbine off again on any other setting.
#  * Best ending: the ruby must be dropped through the clifftop crack BEFORE
#    facing Krelos - that is what sets Vessa "safe", so she survives the blast.
#
# --- Neos: the leaflet (Chanra), the ruby, the potion plant ------------------
read communicator
south
take serpicous rose
southeast
northeast
look at table
take leaflet
read leaflet
drop leaflet
northwest
rip drawing
read scribble
southeast
north
look at shelf
look at encyclopedias
push volume s to u
push volume m to o
push volume d to f
push volume m to o
open secret drawer
take ruby
south
southeast
open cupboard
take cloth
take oil
use oil on cloth
northwest
southwest
northwest
north
clean metal plate
read metal plate
drop cloth
drop oil
# --- fly to Chanra (246, 246) -----------------------------------------------
use flying craft
246
246
south
east
pick leaf
take leaf
west
south
in
speak to high priestess
take wooden bowl
out
north
west
cross bridge
southwest
take shadow ivy
in
southwest
fill wooden bowl
put serpicous rose in wooden bowl
mix wooden bowl
northeast
out
northwest
in
look at table
take silver key
look at wall
look at frame
out
southeast
northeast
cross bridge
east
south
south
in
drink mixture
drop wooden bowl
out
north
north
north
# --- fly to Scython (346, 346): the snowglobe names Palamore ----------------
use flying craft
346
346
east
in
east
cross bridge
south
east
look at shelf
take snowglobe
look under snowglobe
west
north
cross bridge
west
out
west
# --- fly to Palamore (446, 446): stash, telescope, plaque -------------------
use flying craft
446
446
north
in
east
east
look under throne
drop leaf
drop shadow ivy
drop snowglobe
west
out
east
down
look at desk
open drawer
take book of constellations
read book of constellations
drop book of constellations
up
west
in
west
out
up
use telescope
down
west
look under rock
take fladder
up
in
give fladder to follishram
look at plaque
read plaque
out
down
east
south
# --- fly to south Chanra (246, 247): the Crorl and the ocean map ------------
use flying craft
246
247
south
in
north
take crorl
south
unlock chamber door
open chamber door
east
down
press second button
press second button
press visual panel
look at recess
take ocean map
read ocean map
drop ocean map
drop silver key
up
west
out
north
# --- fly to Scython (346, 346): power, the metal rod, the plinth ------------
use flying craft
346
346
east
in
push philosopher statue
down
north
take oil lamp
pull lever
pull lever
south
up
east
cross bridge
south
south
southwest
light oil lamp
turn on valve
northeast
north
north
north
press first button
north
take metal rod
south
south
cross bridge
west
south
look at fountain
insert metal rod
south
put crorl on stone plinth
north
north
out
west
# --- fly to Palamore (446, 446): the Crorl chews the throne wires -----------
use flying craft
446
446
north
in
east
east
put crorl on throne
look at recess
take vial
take shadow ivy
use vial on shadow ivy
take leaf
take snowglobe
west
west
out
south
# --- fly to Q'rel (646, 646): the Crystal Tower -----------------------------
use flying craft
646
646
north
say mountain
open tower door
in
west
use oil lamp on red crystal guardian
east
east
use snowglobe on blue crystal guardian
west
north
west
blow on clear crystal guardian
east
east
use leaf on green crystal guardian
west
in
press first button
press first button
out
up
open cryogenic pod
look under mummified corpse
take crystal key
down
in
press second button
out
west
give shadow ivy to veech
rub floating sphere
east
unlock vault door
open vault door
east
move wall panel
use keypad
1292
take lunastone
use communicator
west
in
press second button
out
south
out
south
# --- fly to Envahr (1646, 1646): the rendezvous -----------------------------
use flying craft
1646
1646
west
up
put ruby in crack
down
northwest
west
in
speak to krelos
speak to vessa
use lunastone on krelos
