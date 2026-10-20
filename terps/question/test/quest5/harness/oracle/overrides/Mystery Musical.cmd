# Mystery Musical ("Escape from Cowellville", ASL 520, 2012): full route to the last room. The game has no `finish` on the good path: answering the opening Ask "yes", then guessing the five instruments opens one locked door after another, and the run ends in the Louis Walsh Room, which has no description and no further puzzle.
# The opening Ask runs inside a SetTimeout, and each quiz is a recurring 130 s (or 120 s) timer that calls GetInput(); qvh answers a prompt a timer script stalls on with the next script line (TickSettled), so `yes` and each instrument name below are those answers. `tick:N` waits out each room's quiz timer, which `speak to` enables.
yes
look
x little mix
speak to little mix
ask little mix about size
ask little mix about famous
ask little mix about made
ask little mix about sound
tick:130
drums
use drum key
e
speak to joe mcelderry
ask joe mcelderry about size
tick:130
double bass
use double bass key
se
speak to diversity
tick:130
piano
use piano key
ne
speak to alexandra burke
tick:120
flute
use flute key
n
speak to matt cardle
tick:120
violin
use violin key
w
look
