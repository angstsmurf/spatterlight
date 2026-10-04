# Quest for the Fountain (2017 revision "051317") — Quest 5, ASL 550, QuestViva oracle
#
# ENDING REACHED: the WIN — `give vial to witch` with the glass vial filled at the
#   Fountain of Youth ("YOU WIN!!!", `finish`). Giving it to the hermit instead is
#   the bad ending. No published walkthrough; derived from game.aslx.
#
# ROUTE: ask the witch about the altar, fetch the glass vial from the rocky shore,
#   return to the altar (its timers raise the smoke ladder), climb to the skystones
#   and jump across them to the castle roof, fill the vial in the fountain room,
#   climb down the castle and walk back to the witch.
#
# SKYSTONE JUMPS ARE RANDOM (seed 1234). Each jump is one RandomChance draw; the
#   third draw of the run is a miss whatever it is spent on, so it is burnt on the
#   low a2<->a3 stones (sw, w, e, w — the fall lands in the river, harmless) before
#   the real climb: n, w, w, w, sw, nw, w, sw (d2 d3 d4 d5 c6 d7 d8 c7).
#   Landing on c7 tips the path and drops the player on the castle roof.
#
# AUTHOR BUG AVOIDED: a missed jump from c6 (or onto it from d5) runs
#   `MoveObject(player, e)`, where `e` is Euler's constant, not a room — the
#   player is left parentless and every later command errors. The route's two
#   jumps through that stone both land.
ask witch about altar
out
e
s
w
take vial
e
n
n
up
sw
w
yes
e
yes
w
yes
n
n
n
up
n
w
yes
w
w
sw
yes
nw
yes
w
yes
sw
yes
sw
take fountain
x vial
ne
down
e
e
ne
give vial to witch
