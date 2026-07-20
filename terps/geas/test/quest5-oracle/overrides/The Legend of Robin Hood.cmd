# The Legend of Robin Hood (Craig Dutton, 2013, v1.4, ASL 540)
#
# Derived from the game source: no walkthrough exists anywhere, and the game's own
# `walkthru` command only says "Available in my IF COMP game file as a WORD document."
#
# Result: THE END (epilogue), state=Finished, errors=0, 170 steps.
#
# This game is why patch_questviva.py section 6 exists. The Sheriff's prize script
# strips your cloak and then does `wait { ... MoveObject(player, cloisters) }`, while
# the archery field's ArcheryContestStarts turnscript calls `finish` on any turn you
# stand there unhooded. QuestViva ran FinishTurn before the wait callback, so the
# turnscript fired in a room the callback was about to leave and the win was
# unreachable; legacy Quest blocks the game thread inside `wait`, so the player is
# already in the cloisters by then. Both engines now defer FinishTurn across a wait.
#
# Footguns this script encodes:
#  - Containers with hidechildren need `look at <container>` before their contents
#    resolve: trough (barn), stall (fair), recess (cottage), stand (library),
#    table (Maudlin's chamber).
#  - `punch sheriff` is ambiguous ("Sheriff's bedchamber" vs "Sheriff of Nottingham")
#    and leaves a disambiguation menu pending that eats the next command.
#    Spell it `punch sheriff of nottingham`.
#  - The cloak MUST be worn before `give two silver coins to archery official`: the
#    ArcheryContestStarts turnscript ends the game on any turn you stand in the
#    archery field unhooded.
#  - Arrow economy: 5 arrows, exactly 4 needed (rabbit, 2 contest rounds, rope).
#  - The cell has a 10-turn starvation timer; eat the skinned rabbit by turn 2.
#  - `take copper key` in the dungeon is easy to miss and is needed 4 chapters later
#    to unlock the casket holding the cursed gold ring.
# Chapter 1 - Sherwood Forest
south
southeast
in
look at trough
take cloth
out
northwest
west
southwest
look at fallen oak
take flint
northeast
east
east
northeast
use cloth on burrow
use flint on cloth
use bow on rabbit
take dead rabbit
southwest
southeast
use bow on deer
# Chapter 2 - the cell
cut dead rabbit
eat dead rabbit
speak to will scarlet
unlock cell door
open cell door
west
look at two guards
take copper key
up
# Chapter 3 - Little John
south
cut branch
north
use quarterstaff on john little
# Chapter 4 - Nottingham Fair
look at fire-eater
speak to allen a dale
look at stall
take iron chain
east
south
in
give iron chain to henry the smith
take pliers
out
look at river
take frogspawn
north
east
pull wheat
cut wheat
west
west
west
use frogspawn on platform
take bent coin
south
look at stone
speak to little john
in
use grain on millstones
take flour
out
north
east
east
in
use flour on floor
speak to samuel the merchant
speak to samuel the merchant
out
in
look at floor
move tapestry
look at recess
take silver coin
use silver coin on bent coin
out
west
west
in
speak to thomas the miller
out
east
north
speak to much
look at cart
take cloak
wear cloak
south
west
northwest
give two silver coins to archery official
use bow on target
use bow on target
speak to sheriff
# Chapter 5 - Saint Mary's Abbey
in
speak to brother tuck
east
kneel
pray
up
push bell
remove mask
# Chapter 6 - the raid on Nottingham Castle
east
pull nail
move horse
west
in
east
east
look at stand
take book
west
west
out
give book to brother tuck
in
west
north
unlock casket
open casket
take gold ring
south
west
open cupboard
take olive oil
use olive oil on gold ring
east
east
out
east
wash gold ring
west
in
north
speak to guy of gisbourne
use bow on rope
up
east
east
speak to marian
west
north
punch sheriff of nottingham
south
west
west
unlock oak door
open oak door
west
take chest
east
north
look at table
take jar of tar
south
east
down
south
out
speak to little john
southeast
turn handle
northwest
up
in
use jar of tar on floorboards
jump from window
east
