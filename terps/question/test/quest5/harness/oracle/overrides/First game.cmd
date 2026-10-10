# The Fall of the Daleks (First game.quest): furthest reachable state, NOT a win (state=Running). The game is shipped broken:
# the Workroom's `down` exit carries a script that only prints "Scrambling down the ladder...", and an exit script replaces the move,
# so the player never reaches the Former Assembly Line and everything past it (grue, Dalek Buster, timestream, TARDIS finish) is unreachable.
# The script collects what can be collected, sabotages the line with the punchcard and shows the dead ladder (d, then look).
x new worker ticket
x receptionist
use new worker ticket on receptionist
take directions card
x directions card
take rations ticket
in
x sign aa
e
in
x ration supply worker
use rations ticket on ration supply worker
take rations
out
n
x sign bb
in
e
x window
x control panel
take punchcard
use punchcard on control panel
d
look
i
