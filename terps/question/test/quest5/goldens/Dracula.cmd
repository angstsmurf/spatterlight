# Dracula (Rod Pike; this .quest = the 2014 unofficial remake by Jon Bardi) -- WINNING SCRIPT
#
# RESULT: state=Finished -- genuine win ("THE END", Dracula staked & beheaded, 290 turns).
#
# SOURCE: the CASA / solutionarchive.com solution (file id,200, by Gunness) is for the
# ORIGINAL 1986 CRL "Dracula", NOT this remake. Same plot & map, but the remake reimplements
# every puzzle with its own parser vocabulary, so the raw walkthrough is heavily adapted here.
# Deviations from the raw walkthrough (all verified against game.aslx, seed 1234):
#   * Systematic verb remaps: "look <obj>" -> "examine <obj>" (no "look X" verb in the remake;
#     only look-at/x/examine); "touch <obj>" -> "feel <obj>".
#   * Dropped the redundant opening "e" (the coachman already blocks; "pay coachman" works first).
#   * Reception desk: "examine desk" FIRST reveals the (initially invisible) bell+register, then
#     "ring bell"+"sign register" yield the room key ("look desk" was rejected).
#   * Part 1->2: answer the death-coach driver "no" (NOT "yes" = ravine death); at the bench
#     "sit"+wait x4 for the carriage, then "jonathan harker" (not bare "harker").
#   * Coach: "close eyes" (not "wake up") breaks the trance & earns the Silver Cross; wear it to
#     board; escape via lift-seat/insert-cross/turn-cross/open-door.
#   * Castle door bat: "feel mouth". Castle labyrinth: lamp is in lab4; rats kill on the 9th maze
#     turn, so the maze path is minimal. Wardrobe->secret stairs: open wardrobe, "move rail"
#     (not "lift"), "move wardrobe"; carry the lamp down but DROP it before the rope descent.
#     Deal with Dracula: "drop cross" in the open box, then "south" x3 (a counter) -> Part 3.
#   * Part 3: sitting-room chair -> coat+money; store "pay storekeeper" (not "buy newspaper");
#     post office "examine s" (mail slot S = Seward's letter, NOT look-south); "sleep" to pass
#     the night; rail "stratford" then platform->bridge->up-line; cab "hawkins".
#   * Renfield: "climb tree" then "drop net" (not "throw net"); Van Helsing drugs him.
#   * Finale: stone -> "smash window" for the axe; "chop tree" (not "fell"); give the axe to the
#     woman for garlic; cross the felled tree; stake = spade "handle" sharpened with the kitchen
#     "carving knife"; front hall: drop garlic, "empty sack", wait for the sunbeam, "focus monocle"
#     to burn the crypt door open; crypt: "drop corn" in the boxes, wait for Dracula to recoil from
#     the garlic, up to the tomb, "get remains", then the tunnels E->mausoleum where the stake wins.
pay coachman
e
s
examine desk
ring bell
sign register
n
e
sit
read menu
lamb
water
w
u
unlock door
open door
n
close door
lock door
close window
sleep
wait
wait
wait
wait
unlock door
open door
s
d
w
no
sit
wait
wait
wait
wait
jonathan harker
look around
examine eyes
close eyes
wear cross
wait
wait
wait
wait
wait
wait
yes
board coach
lift seat
remove cross
examine door
insert cross
turn cross
open door
examine hold
w
s
look around
u
examine frame
examine bat
feel mouth
look around
examine table
get tray
n
get cloth
polish tray
w
w
n
look around
open door
n
lift rail
s
move wardrobe
s
wait
wait
wait
wait
wait
wait
examine table
get bottle
smash bottle
w
s
e
examine window
cut cord
w
w
get cord
n
drop cord
e
w
get cross
wait
wait
wait
wait
wait
wave cross
e
s
e
s
w
get lamp
n
e
e
n
w
n
get cord
open wardrobe
n
move rail
s
move wardrobe
down
down
down
drop lamp
west
tie cord
wait
down
move carpet
open trapdoor
down
open box
drop cross
south
south
south
w
w
examine chair
get coat
examine pocket
e
s
e
e
n
pay storekeeper
read newspaper
read newspaper
e
n
w
examine s
e
s
s
n
w
w
w
n
e
e
sleep
w
w
s
e
e
e
s
stratford
w
s
s
hawkins
w
w
w
n
e
e
sleep
w
get notes
examine desk
get key
drop notes
w
u
unlock door
look around
get net
w
w
e
d
s
w
w
w
w
climb tree
drop net
w
s
w
w
w
n
w
n
w
s
e
s
s
s
w
n
w
get stone
s
s
e
e
e
e
smash window
w
w
w
w
w
s
e
s
chop tree
w
s
give axe
n
e
up
n
w
down
get handle
s
get sack
get hay
n
e
e
n
open drawer
examine cupboard
sharpen handle
s
e
e
drop flowers
empty sack
wait
wait
wait
wait
wait
wait
wait
focus monocle
down
drop corn
wait
wait
wait
wait
wait
up
examine tomb
get remains
e
e
e
e
