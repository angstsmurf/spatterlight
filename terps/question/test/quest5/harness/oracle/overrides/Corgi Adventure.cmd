# Corgi Adventure (v1.0, Big Corgi): a one-room joke game, played to its furthest non-fatal state (state=Running).
# The calendar (the room's light source), the corgi (take, drop refusal, use = pet), the radio (turn on/off; `open` is refused,
# but the battery compartment with the "two batteries" shows once the radio is examined), the bed, and `tell corgi`. The only authored ending is an
# overdose game-over (`sleep … two batteries` on the bed); it is deliberately not taken. Eating the corgi just
# prints a message and does not finish.
look
x calendar
x corgi
take corgi
drop corgi
use corgi
tell corgi about the bed
x radio
turn on radio
turn off radio
open radio
look
x battery compartment
x two batteries
take two batteries
x bed
inventory
eat calendar
