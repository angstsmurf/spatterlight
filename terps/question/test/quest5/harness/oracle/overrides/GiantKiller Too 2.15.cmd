#!errorlimit=200
# GiantKiller Too 2.15 (Quest 5, ~17300-line game.aslx); no published
# walkthrough exists anywhere -- derived entirely from source.
#
# `#!errorlimit=200`: every beanstalk/castle-exterior grid room hits a
# harmless engine bug (Grid_GetGridCoordinateForPlayer/DictionaryItem lookup
# on an unset "x"/"y"/"z" coordinate) on room description; these fire twice
# per crossing and plateau at 45 total by the end of this script -- real
# engine noise, not a derivation defect, but well past the default limit.
#
# Front half: fetch the spade (Barn) and birdseed (Buttercup Betsy, forest
# clearing); `drop birdseed` at the chasm feeds the gerfalcon, which flies
# you across. The lake causeway is a post-hopping plank/spade puzzle: move
# the plank between posts B/A/C to reach post C, then bridge the final gap
# post-C-to-causeway with the spade itself (`move spade to causeway`/`go to
# causeway`) since the plank doesn't reach. On the island, coordinates are
# entered as bare numbers after a directional command (`north`/`40`,
# `west`/`32`) to dig up Barnacle Bill's chest (a plastic token). Returning,
# `dig garden` finds a rusty key; `search straw` (Barn) finds a candle;
# `light candle`+`unlock trapdoor`(key, which breaks)+`open trapdoor`+`down`
# reaches the cellar, where `use token` unlocks the door into the
# VENN-CUBE cave -- an optional side puzzle (not required for anything else,
# just flavour/an achievement) walked through and exited via the `east`
# exit under the wall-carving in the room with rough-red walls.
#
# Market: `drop birdseed` again feeds Buttercup Betsy's clearing NPC once
# more isn't needed; instead `take mushroom` (after `look garden`, which
# reveals it) is needed later as a container for the clown's tears. Three
# fairground challenges must each be beaten (any order) to eventually win a
# "prize" (never an actual pig -- the market always swaps it for something
# else): the Magic Curtain Maze (compass-direction curtains, walked via
# trial exploration to the `NW` exit that ends it) loses the "pig" but wins
# a lamp; the Down-to-One number game (guess 27, a Collatz-sequence number
# that takes >30 steps to reach 1) wins nothing extra but its resulting
# tears (`take tears`, stored in the mushroom) are needed for the bean
# ritual; the Grand Challenge tent bundles three sub-puzzles under one
# entry fee -- Mirror Maze (reflection-size clues, walked by trial), the
# Calculator ("what do I multiply 4 by to get 100 point something" -> 25.1,
# since 4*25.1=100.4), and the Bottle crate puzzle (fill an even count in
# each of 4 rows x 6 columns using U/D/L/R to move + P to place, C to
# check; 18 bottles placed in the pattern above satisfies "even in every
# row and column") -- completing all three wins a small box (a bean).
#
# The Indoor Market partition puzzle (`north` from the west market side)
# needs five partitions placed at specific quadrant walls while navigating
# a 4-room diamond (SE/NE/SW/NW corners): the only route that clears all 5
# without ever picking an already-built or nonexistent wall is
# east(at SE)->north->south(at NE)->west->east(at NW)->south->east(at SW)
# ->north->south(at NW again) -- yields a cup of barley tea.
#
# Bean-ritual assembly (any order, in the tea cup): `put rock in cup`
# (the color/size/texture-cycling "strange rock" from the Venn-Cube cave
# dissolves into soil), `put tears in cup`, `put bean in cup`, then
# `rub lamp` summons a genie who germinates it (taking the entry-fee penny
# as his fee) -- the shoot must then be `replant`ed in the home Garden
# (`replant bean`) to grow the beanstalk that reaches the clouds.
#
# `drop spade` before climbing (it's needed later, dropped in the Garden,
# for the final ending) -- climbing "up" the beanstalk twice reaches the
# castle door; a second beanstalk round-trip (after fetching cheese from
# the cottage) is required because the mouse guarding the base tunnel
# eats it. Castle exterior: `southeast`,`east`,`northeast` from the front
# door circles round to the east-side mouse hole; feeding it `throw cheese`
# lets you pass; the tunnel system beyond runs
# north->north(west/south fork)->west->west into the Giant's Kitchen,
# where the pet ferret gives chase on sight -- retreating east into the
# tunnel fork and `north` into the small dark cavern (its only exit is back
# the way you came) and `wait`ing twice lets the ferret run past, after
# which the whole tunnel route (including the Mouse Lair `south` exit, its
# lockmessage notwithstanding) stays freely passable both ways.
#
# The grappling hook is nested inside a `skeleton` object (Castle North
# West, reached via the west-side castle exterior loop: from the mouse-hole
# entry room, `east`,`north`,`north`,`north`,`west`,`west` reaches Castle
# West, then `north` reaches Castle North West) with `hidechildren` --
# `look at skeleton` first, THEN `take grappling hook` (the bare "take
# hook" fails, "Sorry, what was that?"). Back at the Kitchen, `climb drawer
# unit` (requires the hook) reaches the Middle Worktop.
#
# `take tart` (Middle Worktop) permanently sets off the alarm and welds
# both mouse-hole tunnel exits shut (a one-way trip from here on) -- taken
# deliberately AFTER the hook round-trip. West reaches the pig cage
# (Peppermint): `feed pig`/`yes` starts a 3x3 magic-square puzzle (move
# with U/D/L/R from the top-left cell, `P` places the next sequential
# number 1-9 at the current cell; win when every row/column/diagonal sums
# to 15, i.e. the classic Lo Shu square 2 7 6 / 9 5 1 / 4 3 8) -- the move
# list above is the exact 30-token solve, freeing Peppermint.
#
# East twice reaches the Right Worktop, where visiting it (now that the pig
# is free and the alarm is on) reveals a `gap` under the books: `enter gap`
# starts a "three initials" letter-maze puzzle (three random
# giant's-name-initial letters, drawn with replacement from templates
# E/H/K/R, concatenated into the keypad's unlock code) -- each letter's
# graph always enters at its own position 5, and every exit choice is
# reported live ("Currently, you can go..."), so the shown branching alone
# identifies which of the 4 templates is current; this run's actual draw
# was K, K, K (code "KKK"), reached via each letter's N,SE,E exit path.
# `climb books`+`use keypad`+the derived code disarms the alarm.
#
# Getting back down with Peppermint requires her TIED (`tie pig`, needs the
# hook) before `use apron` (the worktop's descent mechanic) -- an untied,
# merely-following pig is left behind when descending this way (unlike
# ordinary room-to-room exits, which auto-carry a following pig). The
# script below does the descent once without tying first, notices
# Peppermint stayed up top, climbs back up (`climb drawer unit`,`east`),
# ties her, and redoes `use apron` to bring both down together.
#
# The security "box" (Giants Hall, `south` of the Kitchen) is a
# multi-option Eulerian-path puzzle: `look at box` first (hidechildren, as
# with the skeleton) reveals its `button`; each `push option button` cycles
# the display A->B->C->D->A. Of the 4 fixed 14-node graphs baked into the
# source, only option C (reached with 2 pushes from the default A) has both
# an Eulerian trail (exactly two odd-degree vertices, 7 and 10) AND an
# entry point (7) that is itself one of those two odd vertices -- options
# A/B have 4+ odd-degree vertices (no Eulerian trail at all) and D's trail
# doesn't start at its entry point. `climb box` (with Peppermint reachable)
# starts the puzzle, which shows the current node's valid exits every turn;
# paths_taken hits its win threshold (18) on the 19th VALID direction
# pressed, whether or not that direction's edge was already used, so the
# N E N E S W S W S E N E N E S W S E N sequence below (an 18-edge
# Hierholzer walk through option C's graph, ending at node 14 where the
# sole unused edge, N->10, is still a legal press for the free-win 19th
# input) solves it outright and unlocks both castle exits.
#
# Endgame chase: crossing the flap (`west` from Giants Hall) auto-grabs any
# reachable Peppermint and starts a hard 4-events-apart timer chain
# (event4..event7) that catches you if you're still in Castle West at
# +4 turns, still at Beanstalk Top at +5, still at Beanstalk Bottom at +6,
# or still in the Garden with the chase unresolved at +7 -- so Peppermint
# MUST be tied (`tie pig`, hook) before crossing, since Beanstalk Top's
# `down` exit refuses an untied carried pig outright, but `use_spade_in_
# garden` (the actual win trigger) equally refuses if she's STILL tied
# ("can't use the spade with Peppermint on your back") -- she must be
# `untie pig`d again only once safely in the Garden. Castle West's
# `southwest` exit leads directly to Beanstalk Top (1 move), then `down`,
# `down` reaches the Garden with turns to spare. The win command itself is
# `dig garden` (the direct verb->use_spade_in_garden binding on the
# garden/ground scenery object) -- a bare `use spade` instead triggers an
# ambiguous "with what?" disambiguation prompt that wastes a turn and
# risks the chase timer, so it is deliberately avoided here. `dig garden`
# ends the chase, uproots the beanstalk with the Giant on it, and prints
# the full ending through THE END. There is no `finish` call anywhere in
# the game, so the final state is Running, not Finished -- THE END is the
# win, and the script parks on the post-ending "check your score / read
# the epilogue / look around / quit" menu (MOUNTAIN SKI/Moquette pattern).
#
# errors=45 (static grid-coordinate engine noise, see above), steps=495,
# deterministic, ends THE END, state=Running.
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
wait
wait
wait
wait
wait
take rock
down
east
north
east
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
up
northeast
look
southwest
down
down
north
take cheese
inventory
west
take cheese
inventory
east
south
up
up
northeast
southeast
east
northeast
west
inventory
north
throw cheese
north
north
north
west
west
look
east
east
north
wait
wait
look
south
west
west
look
east
east
south
south
look
south
look
east
look
southwest
west
northwest
north
look at skeleton
take grappling hook
inventory
south
southeast
northeast
west
north
north
north
west
west
look
south
southeast
east
northeast
west
north
north
north
west
west
look
inventory
climb drawer unit
look
inventory
take tart
west
look
feed pig
yes
R
R
D
P
L
L
U
P
R
D
D
P
L
P
R
U
P
R
U
P
L
P
R
D
D
P
L
L
U
P
look
inventory
east
east
look
enter gap
N
SE
E
N
SE
E
N
SE
E
look
climb books
use keypad
KKK
look
down
use apron
look
inventory
climb drawer unit
east
look
tie pig
use apron
look
inventory
south
look at box
push option button
push option button
climb box
n
e
n
e
s
w
s
w
s
e
n
e
n
e
s
w
s
e
n
look
inventory
tie pig
west
southwest
down
down
untie pig
dig garden
look
inventory
