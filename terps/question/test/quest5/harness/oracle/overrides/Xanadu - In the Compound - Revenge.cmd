#!errorlimit=500
# Xanadu - In the Compound - Revenge (K. Schaeffer, Quest 5.6.2, 2016)
# WINNING walkthrough for the QuestViva headless oracle (qvh), seed 1234.
# Ends state=Finished via the game's own `quit` (request Quit) AFTER the genuine
# good ending: "Congratulations! You have managed to get through Xanadu, Part II"
# (hut room enter script; the game never calls `finish` on the win path).
# Final score 92% (253/275). Deterministic; reruns byte-identical.
#
# BENIGN ERRORS (~65, all "Script execution depth exceeded 200"): the game's own
# HelperOpenObject/HelperCloseObject are called from onopen/onclose handlers
# (e.g. `black car hood`, Garage room enter/onexit call HelperCloseObject on it),
# which re-fires the changed-isopen handler => bounded recursion, depth-capped by
# QuestViva every Garage visit. Legacy Quest tolerated this; QuestViva's 20-error
# breaker would wedge the session, hence #!errorlimit=500 (storm is finite: a
# fixed number of Garage visits). No gameplay effect; state machine unaffected.
# A handful of deliberate no-op retries exist (extra take marble pairs) as an
# RNG-robustness buffer for the 50% camera-throw check.
#
# ROUTE SUMMARY / GATES:
#  1 Cloud intro: senses chain -> Cold Dark Room (listen/smell/feel, Man Card,
#    reflect light, pull chain) -> Cold Lit Room: summon Blerk (listen/whistle),
#    marble, break camera (50% RNG, retries), fake death -> guard, fob E.
#  2 Prison Cell (fob E): rotten food; eat it in the farthest restroom stall ->
#    janitor rushes off, Janitorial Closet unlocked (whistle/notepad/mint/
#    rat poison/duct tape; glue+spray bottle from his cart).
#  3 Befriend Blerk (mint), become one with Blerk; vent trips launch from the
#    Janitorial Closet (wind gates block upwind routes until the fan dies).
#    Fetch rubber band/dead rat/hairpin from the vent chest; slingshot
#    (widened wrench+rubber band+rat poison) -> Barrack Overhang -> poison the
#    water pool => compound sick, Dining/Showers/Barracks open.
#  4 Disguise: locker 73 pw "Mamouf" (robe+hat); farthest-shower stall event
#    (burn one turn in the stall BEFORE `listen` - a Quest quirk leaves the
#    turnscript counter null otherwise and it dies silently); hair from middle
#    shower drain + glue = beard.
#  5 Dining: tray minus napkin into slot, answer "roonil wazlib" -> Kitchen;
#    wriggling fake rat -> knife. Pantry: baking soda/vinegar/honey/garbage bag.
#  6 Barracks: chair tricks (coins -> vending 503 peanuts; pool dive -> fob D);
#    lunchbox+duct tape+knife+ice+cold bottle => icy2 shorts; Blerk wears them
#    (gets upset; peanuts fix it) -> hot vent passage unlocked (VS "to 33").
#  7 Blerk in Mechanics: post-it #3, flashlight through slot, 4 ATV keys in box.
#    Xanadu: batteries (tank hatch) + flashlight -> lit (dark vents open).
#  8 Blerk: tool belt sash (dark vents), key fob B from Armory Catwalk + reset
#    it in the glass case. Sarlashkar office via hairpin: post-it #2.
#  9 Armory (fob B): landmine plate chips 12406/79545/66866/94772 + clay;
#    wire cutters, stout blade. Chem warehouse (reset fob B): salt->container,
#    TNT, zinc, lithium, isopropyl (MSDS tag codes).
# 10 Blerk axle through Mechanics slot => Commons execution + Infirmary open.
#    Xanadu attends Mongo's execution, grabs dumbbell, waits it out.
#    Axle (now in Garage) wedges the Lab door shut from outside.
# 11 Blerk: dumbbell kills the vent fan; mask from Infirmary; pees in bleach;
#    bleach-pee towel on the grate over the Lab => scientists gassed.
# 12 Lab: bunsen burner + flint lighter; melt ATV keys (tongs), keep one;
#    zinc+TNT mix -> boom thread (honeyed dryer thread). Small Room: Sun Tzu
#    "Art of War", keypad pw "bathory" -> elevator -> Dingo office: post-it #1
#    (all 3 = garage password), hide under desk for the Sarlashkar scene, anklet.
# 13 Blerk: deformed keys back on pegboard (ATVs dead); dryer-sheet rub + water
#    spray clear the static over the max security cell; Strauss revealed;
#    gifts: boom thread, isopropyl, flint lighter => Strauss ready.
# 14 Garage: stab red car tires (stout blade), lithium in radiator (black car),
#    rope hitch+post (SUV), shrunken tube siphon (motorcycle), landmine (tank),
#    => all 6 disabled. Tell Blerk the escape plan, find him under the SUV,
#    use panel, code D@15IN&33GO$62! -> ATV chase -> desert -> village -> hut.
#
# intro
wait
wait
wait
wait
scratch itch
smell blood
focus on pennies
focus on bar of light
focus on sound
focus on feeling of bitter cold
listen
smell
feel
reflect light with card
pull tiny bit of metal
pull chain
listen
listen
listen
whistle
look at grate
ask for help
wait
wait
wait
wait
wait
wait
take marble
throw marble at camera
take marble
throw marble at camera
take marble
throw marble at camera
take marble
throw marble at camera
take marble
throw marble at camera
take marble
throw marble at camera
wait
wait
wait
wait
fake death
move guard
search pocket
hide guard in cell
# to prison cell
w
sw
s
use fob e on door
ne
search rubbage
take rotten food
# to restroom
sw
s
s
se
e
e
ne
n
sw
# get wrench + lunchbox + graffiti name
use nearest stall
look at toilet
open toilet tank
take wrench
out
use middle stall
read walls
read walls
read walls
out
look at trashcan
take lunchbox
use farthest stall
eat rotten food
wait
look
# phase 3: janitor closet loot
w
sw
s
s
s
se
n
look at desk
open drawer
take whistle
take notepad
look at shelves
take mint
take rat poison
take duct tape
s
e
e
ne
n
sw
look at cart
take glue
take spray bottle
ne
s
sw
w
w
n
# phase 4: Blerk in janitor closet
blow whistle
give mint to blerk
ask blerk for help
give notepad to blerk
become one with blerk
up
sw
n
nw
sw
w
s
open chest
take rubber band
n
e
ne
se
s
ne
down
drop rubber band
become one with xan
take rubber band
# phase 5: slingshot + poison the pool
widen wrench
use rubber band on wrench
use rat poison on slingshot
drop rat killer
blow whistle
become one with blerk
take rat killer
up
s
e
ne
e
e
n
n
n
e
look at view beyond compound
use rat killer on small pool
w
s
s
s
w
w
sw
w
n
down
become one with xan
# phase 5.5: fetch dead rat and hairpin via Blerk
blow whistle
become one with blerk
up
sw
n
nw
sw
w
s
take dead rat
n
e
ne
se
s
ne
down
drop rat
up
sw
n
nw
sw
w
s
take hairpin
n
e
ne
se
s
ne
down
drop hairpin
become one with xan
take dead rat
take hairpin
# phase 6: disguise
s
e
e
ne
e
ne
n
search locker 73
Mamouf
look at locker 73
look at keffiyeh
wear robe
enter shower
farthest
wait
listen
use lever
wait
wait
wait
wait
wait
wait
e
# now teleported to Long Hallway; back to showers for hair
e
se
s
e
n
enter shower
middle
look at drain
take hair
e
use glue on hair
use sticky hair
# phase 7: dining silverware + kitchen knife
s
look at tables
take dirty silverware
remove napkin
look at holes
put silverware in slot
roonil wazlib
e
turn on rat
drop rat
look at dishwasher
take knife
e
look at shelves
take garbage bag
take baking soda
take vinegar
take honey
w
w
n
# phase 8: barracks - coins, candy, fob D, spring visible
n
look at chair
move chair
3
stand on chair
wait
take coins
get off chair
use coins on slot
push buttons
503
take peanuts
2
move chair
1
stand on chair
jump in pool
take object
wait
look at object
out
look at cot
use spray bottle on pool
i
# phase 9: sarlashkar office post-it 2 (hairpin)
use hairpin on doorknob
w
look at calendar
read post-it note
e
# garage trip 1: tube, rope, batteries
s
s
sw
use fob d on southeastern door
se
x suv
take rope
open suv door
open console
take tube
open drivers hatch
take batteries
nw
# laundry
w
n
use fob d on eastern door
e
open long cabinet
take dryer sheets
take bleach
take towel
open dryer 2
take long thread
open dryer 1
use tube on dryer 1
use dryer 1
take shrunken tube
use honey on long thread
w
# lunchbox2 + ice + cold bottle
use duct tape on lunchbox
use knife on lunchbox
n
e
n
n
open cooler
take ice
use baking soda on vinegar
use cold bottle on lunchbox
# phase 10: Blerk mechanics trip A (post-it 3, flashlight to slot, keys in box)
s
s
sw
w
sw
w
w
n
drop lunchbox
blow whistle
become one with blerk
take lunchbox
wear lunchbox
become one with xan
blow whistle
give peanuts to blerk
become one with blerk
up
s
e
ne
e
e
n
e
e
s
down
open tool chest
take flashlight
use flashlight on slot
look at tool chest
read post-it note
look at peg board
take box
put keys in box
up
n
w
w
s
s
w
w
sw
w
n
down
drop box
2
become one with xan
take box
2
# phase 11: garage trip 2 - flashlight, SUV rope, motorcycle siphon
s
e
e
ne
e
use fob d on southeastern door
se
take flashlight
open flashlight
put batteries in flashlight
turn on flashlight
tie rope to hitch
tie rope to post
open gas tank lid
put tube in tank
blow tube
nw
w
sw
w
w
n
# phase 12: Blerk - tool belt sash + key fob B reset
drop flashlight
blow whistle
become one with blerk
take flashlight
up
s
e
ne
e
e
n
n
n
w
w
se
wear belt
nw
w
n
n
take key fob b
open glass case
put key fob b in glass case
close glass case
push button
open glass case
take key fob b
s
s
e
e
e
s
s
s
w
w
sw
w
n
down
drop key fob b
drop flashlight
become one with xan
take key fob b
# phase 13: Armory - landmine parts
s
nw
n
n
n
ne
e
use fob b on door
n
look at table
look at dispenser
push buttons
12406
yes
push buttons
79545
yes
push buttons
66866
yes
push buttons
94772
yes
take clay
put clay in plate
look at dispenser
take wire cutters
take stout blade
s
# phase 14: chemical warehouse
e
se
s
s
s
e
use fob b on southern door
s
search cards
N47NED
take salt
out
pour salt
search cards
D08PLO
open crate
get tnt
out
search cards
E90TNA
take zinc
out
search cards
H11ZWA
take lithium
out
search cards
Q01QWD
take isopropyl
out
n
# phase 15: Blerk axle trip -> execution + infirmary unlock
w
sw
w
w
n
blow whistle
become one with blerk
up
s
e
ne
e
e
n
e
e
s
down
take axle
put axle in slot
up
n
w
w
s
s
w
w
sw
w
n
down
become one with xan
# phase 16: Commons during execution - dumbbell
s
nw
n
e
look at weight lifting station
take dumbbells
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
e
# phase 17: fetch axle from garage, block lab door
s
s
e
use fob d on southeastern door
se
take axle
nw
w
sw
w
w
nw
n
n
w
use axle on northern door
e
s
s
se
n
# phase 18: Blerk - fan, mask, bleach-pee towel (gas the lab)
drop dumbbell
drop bleach
drop towel
blow whistle
become one with blerk
take dumbbell
up
sw
n
nw
n
e
n
n
use dumbbell on fan
s
s
w
sw
down
open cabinets
take mask
up
e
se
s
ne
down
take bleach
take towel
pee in bleach
wear mask
up
sw
n
nw
n
e
nw
use bleach pee on towel
put towel on grate
se
w
s
se
s
ne
down
become one with xan
# phase 19: laboratory work + Small Room + Dingo office
s
nw
n
n
w
use fob b on northern door
n
look at lab tables
open drawers
take tongs
take flint spark lighter
take mortar pestle
use bunsen burner on gas spigot
use gas spigot
use flint spark lighter on bunsen burner
put zinc in mortar
use mortar
take powder from mortar
use unknown key 1 on lit bunsen burner
use container of mixed chemicals on long sticky thread
e
search bookcase
sun tzu
read art of war
move plant
use rectangle
bathory
in
push button
out
look at computer
read post-it note
hide under desk
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
take anklet
push button
in
push button
out
w
s
# phase 20: Blerk - deformed keys to pegboard, static removal, Strauss
e
s
s
se
n
drop three deformed keys
drop boom thread
drop isopropyl alcohol
drop flint spark lighter
drop dryer sheets
drop spray bottle
blow whistle
become one with blerk
take three deformed keys
up
s
e
ne
e
e
n
e
e
s
down
put keys on pegboard
up
n
w
w
s
s
w
w
sw
w
n
down
take dryer sheets
take spray bottle
use dryer sheets
up
s
e
n
use spray bottle
down
speak to prisoner
speak to prisoner
speak to strauss
up
s
w
n
down
take boom thread
take isopropyl alcohol
take flint spark lighter
up
s
e
n
down
give boom thread to strauss
give isopropyl to strauss
give flint spark lighter to strauss
up
s
w
n
down
become one with xan
# phase 21: spring, land mine, disable remaining vehicles
s
e
e
ne
e
ne
n
n
cut spring
use spring on plate
s
s
sw
use fob d on southeastern door
se
stab red car tires with stout blade
open black car hood
open radiator
put lithium in radiator
put land mine under tank
nw
# phase 22: tell Blerk the escape plan
w
sw
w
w
n
blow whistle
tell blerk about escape plans
# phase 23: escape!
s
e
e
ne
e
use fob d on southeastern door
se
search suv
use panel
D@15IN&33GO$62!
sw
s
w
wait
wait
wait
quit
