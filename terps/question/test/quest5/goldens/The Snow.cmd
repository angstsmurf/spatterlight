# The Snow — Gareth Meyrick, 2023, ASL 580; derived from game source
# (game.aslx) plus the in-game HINT command (snow_hint), which gives a
# per-room walkthrough nudge. Supplied locally as "The-Snow - By GAZarts.quest",
# renamed to match the in-game title; the game's download page is
# textadventures.co.uk/games/view/c42dt2nvtuw0t17mc8bphw/the-snow-a-text-adventure
# published as "The Snow: A Text Adventure" (see games.manifest.tsv) but it
# hosts no walkthrough. Three finish() sites: the "Barricade of Snowmen" is a
# fatal dead end (walking the path west from Meeting Point instead of north)
# — avoided here. The other two are both genuine, non-fatal endings reached
# by choosing a door in the final scene; this script takes "THE BLUE DOOR"
# (the snowman actively warns against it — "Ignore that door" — but it is
# the deeper reveal: the whole year of snow was a VR simulation in a lab).
# "THE RED DOOR" (the snowman's recommended, more overtly requested door)
# would also finish() from the same choice point.
# Route: closet (coat + penknife) -> garden gate -> break into Number 7 ->
# boil a thermos of water in its kitchen -> scald the snowman blocking the
# street -> school (open entrance with force, then key fob later) -> Class
# Gwyrdd: take the cigarettes from the teacher's desk (this locks the
# hallway's south exit and puts a hostile snowman in the hallway) -> speak
# to that snowman, which teleports you into the locked Class Glas -> its
# teacher's desk holds a lighter and the Tesco key fob; light a cigarette
# and use it on the sprinklers to flood the room and kill the snowman, then
# open the classroom door to reach the (now clear) hallway -> foyer -> use
# the key fob at Tesco's staff door -> emergency exit -> Meeting Point ->
# north (NOT west, which is the fatal snowman barricade) onto the
# motorway -> dig out the license plate of your own buried car, revealing
# your own corpse inside and a final snowman -> ask it about the afterlife
# to summon the red/blue doors -> open the blue door. errors=0.
in
x shelves
open shoe box
take penknife
take overcoat
out
east
down
open front door
south
south
open garden gate
south
south
break in front door
north
open bottom cabinets
take thermos
fill kettle
boil kettle
fill thermos
south
open front door
south
north
east
use thermos on snowman
east
open entrance doors
north
west
open teacher's desk
take cigarettes
east
speak to snowman
open teacher's desk
take lighter
take key fob
light up cigarettes
use lit cigarette on sprinklers
open classroom door
north
open entrance doors
south
west
west
open shop door
use key fob on fob scanner
open emergency exit
north
x ford kuga
dig out license plate
x window of ford kuga
x snowman
speak to snowman
ask snowman about afterlife
x red door
open blue door
