#!errorlimit=200
# GiantKiller Too 2.14 (Quest 5, ~17850-line game.aslx, Quest 5.9.9166.36226);
# no published walkthrough exists -- derived entirely from source, adapted
# from the 2.15 winning script (a later bugfix release: same puzzles, same
# ~591 objects, MD5-distinct game.aslx). THIS RELEASE IS UNWINNABLE -- the
# script below is the best-effort maximal script, not a win.
#
# Front half is identical to 2.15's derivation (see that file's header for
# full puzzle detail): fetch spade/birdseed, cross the lake causeway via the
# plank puzzle, dig Barnacle Bill's island treasure by coordinate, find the
# candle/key, light/unlock/open the trapdoor to the Cellar.
#
# Mother/cellar-gate difference from 2.15: `down_to_cellar` refuses entry
# while `mother.parent = Cottage`, and she only leaves once you've received
# the penny (`talk to mother`) AND physically reach a room with a
# mother-removal trigger. 2.15 has TWO such rooms (West Road 1 AND East
# Road); 2.14 has only ONE -- the Market Entrance's `enter` script. Since the
# 2.15 script's route never passes through the Entrance before needing the
# cellar, this script inserts a 4-move detour (`east,east,west,west` at
# "Road", lines 11-14) to reach and leave the Entrance harmlessly before
# doubling back to the causeway.
#
# THE VENN-CUBE CAVE PUZZLE IS UNSOLVABLE IN 2.14 (confirmed by source proof
# and live testing) -- this is almost certainly why 2.15 exists. The puzzle:
# a carried rock's size/colour/texture (small/large, blue/red, smooth/rough)
# each toggle via one axis of an 8-room cube maze (east/west <-> texture,
# north/south <-> colour, up/down <-> size; confirmed identical room graph
# and flip functions in both versions by direct source comparison). Every
# room's own identity matches one combination exactly (e.g. Upper South West
# Chamber = "small chamber with smooth blue walls"), and every move flips
# the rock's property in lockstep with the matching position axis -- so the
# XOR offset between "current rock state" and "current room's identity" is
# fixed forever at the moment you pick the rock up, and cannot change while
# carrying it (proven by reading all 8 chambers' exit scripts). The lone
# exit (`cube_exit`, Lower North East Chamber, alias "east") requires the
# rock to be the *exact opposite* of that room's own identity (small, blue,
# smooth vs. the room's own large, red, rough) -- i.e. you must pick the
# rock up already in the fully-inverted state relative to your start room.
# That inversion is produced by the `rock_flip` turnscript repeatedly
# calling `advance_rock_property` while the rock sits unclaimed, gated on
# `rock.puzzle_started`. In 2.15, Upper South West Chamber's `<firstenter>`
# sets `rock.puzzle_started = true`; in 2.14 this firstenter script IS
# ENTIRELY ABSENT (confirmed via a full diff of the cube-cave source block
# between the two versions -- `puzzle_started` has no setter anywhere in
# 2.14's ~17850 lines), so the turnscript never fires, no matter how long
# you wait, and the rock is always picked up in its untouched default state
# (small, blue, smooth) -- which exactly MATCHES the start room's identity
# instead of opposing it. With a zero offset locked in at pickup, the rock
# will match whatever room you are standing in FOREVER; reaching Lower North
# East Chamber therefore always leaves it "large, red, and rough" (that
# room's own identity) and `cube_exit` always refuses: "Mysteriously, you
# are forced back from the door. The rock is not in the right state to
# leave with it." Live-tested (lines 68-72: take rock/down/east/north/east)
# and confirmed exactly this. No drop-and-re-pickup, hint dialogue, or
# alternate route bypasses it: the only two things that ever change the
# rock's properties are the position-linked flip (offset-preserving, proven
# above) and the dead turnscript, and the ordinary Cavern-ward exit is
# hard-blocked while carrying the unsolved rock (`if Got(rock) and not
# puzzle_over: forced back`). This is a genuine progression-blocking
# soft-lock, not a missed side puzzle: growing the bean (`Do_genie_puzzle`)
# requires `tears.parent = rock` and `bean.parent = rock`, so with the rock
# permanently trapped in the cave, the entire beanstalk/castle-top arc --
# more than a third of 2.15's winning script (grappling hook, kitchen, pig
# cage, letter maze, security box, the whole chase-escape ending) -- is
# permanently unreachable. The 2.14->2.15 version bump is almost certainly
# this exact fix.
#
# Best-effort script: after demonstrating the failed cube_exit (lines
# 68-72), `drop rock` (line 73) abandons it in Lower North East Chamber and
# retraces out to the Cavern (lines 74-77), then continues on the same path
# 2.15's script takes after its own successful solve (lines 78-81), so the
# rest of the game can be played unencumbered. From there it plays every
# independently-reachable beat from 2.15's derivation that does not require
# the beanstalk: garden mushroom, the fairground curtain-maze teleport
# puzzle, the clown's tears, the Indoor Market partition maze (P/R/L/D
# control pad) for the bean, then confirms the dead end explicitly -- `put
# rock in cup` fails ("Sorry, what was that? (rock)", line 302, since the
# rock was abandoned), the genie refuses ("Call me back when you are
# ready", since tears/bean never reach `rock`'s inventory), `replant bean`
# refuses ("The bean is not ready to be replanted"), and the beanstalk's
# `up` exit (line 315) permanently reports "There is nothing of interest in
# that direction" (`beanstalk_up` is never made visible). The script ends
# there.
#
# errors=0, steps=317, deterministic, state=Running (best-effort -- game
# UNWINNABLE, Venn-Cube Cave soft-lock: missing `rock.puzzle_started = true`
# firstenter in Upper South West Chamber, fixed in 2.15).
yes
north
west
talk to mother
east
east
take spade
west
south
south
east
east
west
west
west
west
north
talk to Buttercup Betsy
south
west
west
drop birdseed
wait
wait
west
north
north
take plank
move plank to post b
go to post b
move plank to post a
go to post a
move plank to post c
walk along plank
move spade to causeway
go to causeway
take spade
north
north
40
west
32
dig ground
open chest
east
32
south
41
south
south
east
drop birdseed
dig garden
wait
wait
north
east
search straw
west
west
light candle
unlock trapdoor
open trapdoor
down
use token
east
east
take rock
down
east
north
east
drop rock
south
west
up
west
west
up
east
south
look garden
take mushroom
south
east
east
northeast
east
north
north
N
NE
E
SE
S
SW
W
NW
N
NE
E
SE
S
SW
W
NW
N
NE
E
SE
S
SW
W
NW
N
NE
E
SE
S
SW
W
NW
N
NE
E
SE
S
SW
W
NW
N
N
NE
E
SE
S
SW
W
NW
N
N
NE
E
SE
S
SW
W
NW
N
N
NE
E
SE
S
SW
W
NW
N
N
NE
E
SE
S
SW
W
NW
N
N
NE
E
SE
S
SW
W
NW
N
SE
N
N
N
N
S
E
SE
NE
S
SW
NE
SE
NE
W
NW
NE
E
S
SW
E
N
SW
SE
N
NE
E
SE
SW
E
E
NE
W
SE
S
SW
SW
W
W
NW
south
talk to clown
27
take tears
inventory
look
north
east
north
N
N
N
W
N
W
W
E
S
W
W
S
S
S
east
25.1
south
south
P
R
P
R
P
R
P
R
P
R
P
D
L
L
P
L
P
L
P
L
P
D
P
R
P
R
P
R
R
P
D
R
P
L
L
L
P
L
P
L
P
C
open box
take bean
west
west
north
west
east
north
south
west
east
south
east
north
south
inventory
put rock in cup
put tears in cup
put bean in cup
rub lamp
inventory
look
south
southwest
west
west
north
replant bean
drop spade
up
look
inventory
