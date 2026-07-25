# Welcome to Pineview (Filthy & Free Publishing, 2016; v1.2, ASL 550; 68 rooms, 656 objects)
#
# No published walkthrough exists.  The 160-step route below was derived
# entirely from game.aslx.  It reaches the game's only reachable `finish`,
# the closing scene on "the same wooded path":  "My name is Casey Adams,"
# the man says.  "And we have got some serious work to do."
#
# There is no score and no inventory limit.  The game is three acts, and
# each act is a strict dependency chain:
#
# ACT 1 (daytime).  The Preceptor Mask (Hornby's mask shop, `speak to
# salesman`) is the key to the whole act:  the pawnbroker only trades for a
# masked customer, the bank teller only talks to one, and Bubba at the
# salvage yard only talks to one.  Order matters at the Pawn Shop --
# `ring bell` runs a four-way if/else on what you are already carrying, and
# the voucher branch is LAST, so the bell must be rung before the motorcycle
# keys exist.  Voucher -> Moon River Church (answer `water` to the one
# `get input`) -> key to box 150 -> the package in the Post Office -> Bubba
# -> the wrench, and the wrench is what makes the Fortune Grounds
# description walk you into your own house, where the nightmare and Kurtz's
# phone call hand over the motorcycle keys.  `ride motorcycle` also wants
# the gold coin, which comes from Sarah's trailer.
#
# ACT 2 (the University).  A deliberate chicken-and-egg:  both exits into
# the Office are gated on the Optimist Mask, and the Optimist Mask is inside
# the Office filing cabinet.  The way in is `search junk` in the third-floor
# Storage Room, whose nightmare teleports you into the Office.  Escaping the
# building then needs three separate things at once -- `pull rope` behind the
# Auditorium curtain (the only `UnlockExit(exit)`), the painting *Through The
# Fire* from the darkened room behind the sixth dorm door, and the
# wirecutters from the Darkroom.
#
# ACT 2 (night) -> ACT 3.  Rosalin in Chapman's Diner sends you to the old
# Train Station; the tracks are blocked by a fire (`use fire extinguisher on
# fire` -- the extinguisher hangs in the Moon River Storage Facility), and
# `take briefcase` there arms a SetTurnTimeout(6) whose payoff is the
# Two-Way Radio and Officer Volgin's request for an ambulance.  That is the
# authored reason to walk back into Pineview Hospital, which swaps the `a01`
# night marker for `aa1` and runs the Act 3 wake-up.  In Act 3 the Police
# Station description branches on that marker and drops you on the wooded
# path for the ending.
#
# TRAP THE ROUTE AVOIDS -- the Police Station at NIGHT.  With `a01` its
# description is an abduction:  you wake in the Hidden Cellar, pick up the
# desmian mask, and are dumped on Huffman Avenue 04.  It reads like a bonus,
# but the Huffman Avenue 02 exit script is `if (Got(desmian mask))` -> "You
# don't really feel like getting kidnapped again", and the mask cannot be
# dropped.  Taking it therefore locks the Police Station shut for the rest
# of the game and makes the ending unreachable.  Other deaths and dead ends
# left alone here:  `take jar of pickles` on Main Street 03 arms a
# SetTurnTimeout(19) death; `use dirty rag on paint` at home starts the
# Repairman fight; drinking from the Train Station fountain adds
# `waterpoison`; The Gateway, `end`, the Security Checkpoint and the Padded
# Room endings have no reachable entrance.
#
# NAVIGATION FOOTGUN, and the reason every move here is spelled `go <word>`:
# exits carry multi-name alias lists (alias="left; out"), which OVERRIDE the
# inherited direction alias.  Bare `left` is "I don't understand your
# command" and bare `west` is "You can't go there", but `go left` works
# because Quest splits the `;`-separated list.  Where two exits in a room
# share a word the parser raises "Please choose which 'in' you mean" and
# wastes the turn, so the route uses unique aliases instead:  `go downstairs`
# and `go to auditorium` / `go to restroom` on the Third Floor Landing, and
# `go to diner` on Main Street (whose parking-lot exit also carries "in
# parking lot").  `take key` in the Darkroom is likewise ambiguous once
# Kurtz's Key is in hand -- `take copper key`.
#
# TIMERS.  Quest timers are real-time, so the four SetTimeout chains on this
# route need explicit `tick:` steps:  two `tick:3` at the trailer park (the
# doorbell, then Sarah's own arrival) and four `tick:2` in the darkened room
# (a nested 1/2/2/1 cascade).  `SetTurnTimeout` is turn-based and fires by
# itself:  the sixth-door scare wants two filler turns, the briefcase six.
#
# No RNG anywhere on this path; errors=0.
x class notes
go left
go right
go forward
go forward
go forward
go right
go in
speak to salesman
go back
go back
go back
go back
go back
go north
x bushes
take wallet
open wallet
go left
ring bell
go out
go south
go right
go in
water
go back
go north
go right
unlock box 150
open box 150
take package
go out
go north
go right
speak to attendant
go south
go south
go right
go forward
go forward
go forward
go right
go forward
go down
speak to bubba
go right
ring doorbell
tick:3
tick:3
i believe so
go back
go back
go back
go back
go back
go back
go back
go north
go north
go forward
go left
go down
go south
go south
go back
ride motorcycle
go forward
go up
go up
go forward
go right
search junk
open filing cabinet
open bottom drawer
take darkroom key
open top drawer
read note
sit on chair
go right
go downstairs
go down
go right
go up
go up
go to restroom
x left stall
search garbage
go out
go forward
use telescope
yes
yes
go left
switch on light
take wirecutters
take copper key
go out
go right
take dolly
go out
go back
go to restroom
use dolly on statue
go out
go downstairs
go down
go right
go up
go in
open fifth door
search toilet
open sixth door
look
look
tick:2
tick:2
tick:2
tick:2
go up
go to auditorium
pull rope
go left
go left
go down
go back
go back
go to diner
go out
go right
go forward
go forward
go forward
go in
take fire extinguisher
go out
go back
go right
go forward
use fire extinguisher on fire
go forward
take briefcase
look
look
look
look
look
look
go back
go back
go back
go back
go back
go back
go back
go in
go left
go right
go forward
go left
go forward
