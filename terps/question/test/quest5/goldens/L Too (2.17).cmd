# L Too (2.17) -- WINNING SCRIPT (MAXIMUM score, no hints)
#
# RESULT: state=Finished, errors=0. The game's win: place the ebony sphere on the
# treasure-chest indentation, take the books, `say neumann` in the Treasure Room
# -> "...completed the game successfully!!! THE END." Final certificate reads
# 36/35 puzzles, 1500/1000 points -- the +500 "Avoiding Alarm in Treasure Room"
# bonus (earned by seating the sphere before lifting the books) pushes the score
# past the 1000 nominal max. No hint is ever used (hints cost points).
#
# L Too is a Quest 5 re-implementation of the 1984 BBC Micro game "L: A
# Mathemagical Adventure". NO published walkthrough exists; this script was
# derived entirely from game.aslx.
#
# HARNESS NOTES specific to this game:
#  - Only the very first prompt ("Sound effects on?") is a real Core `ask`
#    (answer:no). Every later yes/no is the game's custom get-input -- answer with
#    a bare `yes`, never answer:yes.
#  - Nested/hidden objects (piano keyholes; the guard's coat->pocket->pen) only
#    become referenceable once their PARENT has been examined -- hence the
#    `look at piano/lid/keyholes/...` and `look at drogo robot guard/coat/pocket`
#    steps before putting keys / taking the pen. This is Quest discovery scope,
#    not padding.
#  - The Swimming Pool `down`/`in` chokepoint relies on the same-value
#    changed<attr> guard (native commit ac07604d / oracle patch 94e6312a); without
#    it enter_pool recurses forever.
#
# PUZZLE SOLUTIONS (all derived, none from hints):
#  - Telephone: Fibonacci call list, dial 610 twice; chest code 4181 (1d4 seed).
#  - Cellar maze -> Palace Exit; jail guard 169 -> `say thirteen`; maze guard 144
#    -> `say twelve`. Turtle: move AWAY to close the doubled gap (w,s,s,s,s).
#  - Pig is uncatchable by parity if you move first -> answer `no`, then n,e,n.
#    Magic word `neumann` (also teleports out of the Green wing).
#  - Key blank filed per Drawing-Room diagram; snooker tightest angle = 3;
#    cook TOLT/FIMA/MUOT = 6,6,6 (max height 26); lights 4,2,1,2; code keypad
#    25468; bath bungs by shape; orchard 9-trees-10-rows pattern; ape->awe->owe->owl.
#  - Piano: bronze->right, silver->left, gold->middle keyhole; unlock; play
#    "three blind mice" -> shrinking bottle. Drink -> squeeze through pool hole.
#  - Upper level: safe 4096 -> batteries -> calculator; bat wants a triangular
#    number 30-90 (36) -> tin hat; spider Eulerian web RRLRRRLRRRLLRLRRRLRRRLR
#    -> leggings; computer recursion (look x4 -> stack overflow, turn off x3) ->
#    vest; guard-in-corridor trick frees the pen; write "121" on the vest and wear
#    hat+sunglasses+leggings+vest for the Drogo disguise; the DEAF guard (121 =
#    11^2) is scared by the broken calculator reaching 11 (7-4*4-4*4-7-7-7) ->
#    drops the access card -> Guardroom -> Treasure Room.
answer:no
north
north
look at sink
look at plughole
take plug
east
speak to abbot
yes
up
look at dust
down
west
west
look at benches
take metal
west
north
east
use telephone
610
610
open chest
4181
down
north
east
east
south
take sphere
east
say twelve
east
north
north
speak to turtle
c
w
s
s
s
s
south
south
west
north
west
west
west
south
up
look at walls
shout
say thirteen
east
east
catch pig
no
n
e
n
say neumann
north
east
east
east
use file
yes
F
R
F
R
F
R
F
R
F
R
F
R
F
R
R
F
D
L
L
L
L
F
D
L
L
L
F
R
F
R
R
R
R
F
R
F
R
F
C
take octahedron
north
east
up
south
east
use cue
3
east
down
speak to cook
yes
6
6
6
up
up
east
speak to electrician
yes
4
2
1
2
take oar
west
down
west
south
west
wear sunglasses
use keypad
25468
take dodecahedron
east
east
remove sunglasses
north
west
north
down
north
open bag
take icosahedron
put cube in rectangular hole
put tetrahedron in triangular hole
put octahedron in square hole
put dodecahedron in small five-sided hole
put icosahedron in large five-sided hole
push bath
northeast
speak to gardener
yes
P
E
E
E
E
P
S
W
W
P
S
P
W
W
P
E
E
E
E
P
S
W
W
P
S
W
W
P
E
E
E
E
P
C
southwest
push bath
yes
yes
awe
owe
owl
push bath
south
up
south
west
west
west
west
down
west
south
west
south
take ladder
north
east
north
east
up
east
use ladder on statues
down
take gold key
up
east
east
look at piano
look at lid
look at keyholes
look at right keyhole
put small bronze key in right keyhole
look at left keyhole
put small silver key in left keyhole
look at middle keyhole
put small gold key in middle keyhole
unlock piano
play piano
three blind mice
take bottle
drink bottle
west
west
down
in
south
up
east
north
take calculator
south
northeast
open safe
4096
take batteries
southwest
use batteries on calculator
southeast
turn on calculator
speak to large bat
36
northwest
east
east
speak to spider
yes
say start
R
R
L
R
R
R
L
R
R
R
L
L
R
L
R
R
R
L
R
R
R
L
R
west
west
southwest
look at screen
look at screen
look at screen
look at screen
turn off computer
turn off computer
turn off computer
northeast
northwest
west
east
west
look at drogo robot guard
look at coat
look at pocket
look at pen
take pen
east
write on vest
121
wear hat
wear leggings
southeast
south
use calculator
7
-
4
*
4
-
4
*
4
-
7
-
7
-
7
take card
wear sunglasses
south
west
open chest
put sphere in chest
take books
say neumann
look at certificate
quit
yes
