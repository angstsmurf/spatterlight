# Lost Hearts: the intended ending (state=Finished). Key from the dust, torch reveals the hidden door (skull), bone dagger from the crack,
# cut the membrane, touch the orb, orb into the round hole, skull and dagger on the heart; the red figure says "use the knife".
# The player object has no "me" alias in this game, so the last command names it: `use bone dagger on player`.
x door
search door
search dust
take old key
use old key on door
n
x torch
take torch
open hidden door
take skull
x skull
n
nw
search crack
take bone dagger
se
e
use bone dagger on membrane
n
nw
x glowing orb
touch glowing orb
se
ne
use glowing orb on round hole
d
x heart
use skull on heart
use bone dagger on heart
x red figure
ask red figure about who
ask red figure about exit
ask red figure about knife
use bone dagger on player
