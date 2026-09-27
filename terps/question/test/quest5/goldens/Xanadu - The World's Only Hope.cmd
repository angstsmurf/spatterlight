# Xanadu - The World's Only Hope (Kevin Schaeffer, 2015) — WINNING script
# Ending: enter the Guard Barracks via the secret platform in the NE corner of
#   Dingo's base; after 2 turns a SetTurnTimeout fires the capture/twist scene
#   ("Congratulations!  You have completed the first installment ... 'Xanadu -
#   The Surprise'") and calls finish.  state=Finished, errors=0.
# Gates: belt knife+lasso -> sticky lasso (bugs behind sand dune) -> Dirty Key
#   across the Chasm -> town gate padlock; shovel from Small Cave (tick:30
#   light-sliver timers); sand in white cloth + odd machine (tick:40) -> glass
#   bowl -> water -> pour on root -> giant tree; copper wire strung machine->
#   lab->chimney->rooftops->desk->jail, tied to cell door, machine on ->
#   charged door electrocutes the summoned henchman (red button + break window
#   with shovel, hide in cell, tick:24) -> gold key -> vault chest (Velma code
#   "answer 5326") -> monocular + velvet -> Clean Spyglass; rope+rock ->
#   climbing rope up tree; "use spyglass" at top reveals base code 28744,
#   awards Man Card and unlocks the thicket maze; maze items (triangular rock+
#   plank+vine -> lever; hollow stick+white pod -> pod launcher); pod launcher
#   on warning sign removes patrol guard; lever+jump flips boulder -> security
#   panel; type 28744 -> circle of dirt; stand on circle; push loose section.
# Timers are authored recurring timers, driven explicitly: tick:30 (cave light),
#   tick:40 (odd machine), tick:24 (henchman-at-the-jail sequence).
# Benign quirk: "use dirty key on padlock" leaves Dirty Key1 in inventory (the
#   author's script removes the wrong twin object) — we just drop it.
# Zero script errors, zero unrecognised commands.
#
# --- intro: ride out the ambush (turnscript moves us at turn 9) ---
wait
wait
wait
wait
wait
wait
wait
wait
wait
# --- Rocky Outcropping: knife + lasso from the belt ---
look at belt
take knife
take lasso
down
west
# Small Cave: let the sliver of light creep (SC timers) to reveal the shovel
tick:30
look at glint
take shovel
east
south
# --- desert: gum up the lasso on the sticky bugs behind the dune ---
east
hide
kill bugs with lasso
emerge
south
# --- Quarry/Chasm: open the mine, lasso the key across the chasm ---
east
look at planks
use knife on nails
east
use sticky lasso on dirty key
take dirty key
drop sticky lasso
west
west
# --- town gate ---
southeast
take cloth
use dirty key on padlock
east
# --- Old Miners Shop: screwdriver + rope ---
northeast
take screwdriver
take rope
southwest
# --- fetch sand in the white cloth ---
west
northwest
north
use white cloth on sand
south
southeast
east
# --- odd machine: sand -> small glass bowl (OM timers, tick:40) ---
southeast
open machine
use white cloth on plate
close machine
turn on machine
tick:40
open machine
take small glass bowl
drop dirty key
# --- fetch water, revive the root, grow the giant tree ---
northwest
west
northwest
north
west
down
northeast
use small glass bowl on water
southwest
up
east
east
use water on root
drop small glass bowl
west
south
southeast
east
# --- Sheriffs Office: wire, desk, roof access ---
south
south
look at desk
open square drawer
take spool
unwind spool
push desk east
stand on desk
open access panel
climb down
north
north
# --- rooftop route: lab -> chimney -> roof; drop the scaffolding bridge ---
southeast
south
move logs
in
up
look at scaffolding
use screwdriver on screws
push scaffolding
drop screwdriver
down
out
north
# --- string the wire from the machine to the jail cell door ---
put end of wire on plate
south
in
up
west
down
climb down
north
in
tie wire to cell door
out
north
southeast
switch on machine
northwest
south
# --- summon the henchman and electrocute him on the charged door ---
south
push red button
north
break window with shovel
in
tick:24
search corpse
take gold key
out
south
push blue button
north
north
# --- bank vault: Velma's password, chest, spyglass ---
east
push green button
answer 5326
northeast
use gold key on durable chest
take monocular
use knife on cloth lining
clean monocular with velvet
drop gold key
drop velvet
southwest
west
west
northwest
# --- climbing rope, up the tree, read the base code (28744) ---
east
take rock
take plank
tie rope to rock
west
throw rope at bough
up
up
up
up
use spyglass
down
down
down
down
# --- through the thicket maze (Man Card) ---
north
east
northeast
east
east
north
north
# side trip: triangular shaped rock (LTP4) -> loose fulcrum
southeast
south
east
east
take triangular shaped rock
use plank on triangular shaped rock
west
west
north
northwest
# side trip: vine (LTP22) -> lever
south
west
north
north
north
southeast
north
east
take vine
use vine on loose fulcrum
west
south
northwest
south
south
south
east
north
# side trip: hollow stick (LTP12) + white pod (dead end) -> pod launcher
east
take stick
east
southeast
north
northwest
northeast
north
take pod
use white pod on hollow stick
south
southwest
southeast
south
northwest
west
# main line to the waterfall and the patrol area
north
northeast
north
northwest
west
southwest
northwest
northwest
# --- endgame: sleep-dart the sign, flip the boulder, punch the code ---
use pod launcher on warning sign
west
use lever on large boulder
jump on lever
type 28744
stand on circle
push loose section
# Guard Barracks: two turns until the capture scene fires finish
wait
wait
