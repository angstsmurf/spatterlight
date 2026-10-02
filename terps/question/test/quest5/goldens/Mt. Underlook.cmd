#!errorlimit=200
#!clock=5
# Mt. Underlook: Trial of False Eyes (Quest 5, ASL v580, 25564-line game.aslx,
# 1294 objects); no published walkthrough exists -- derived entirely from
# source. A FULL WIN: ends "You have completed Mt. Underlook!" ([state=
# Finished]). Act 1 needs the `#!clock=5` typing clock (REAL-TIME CHASE below);
# so do the Grand Admiral gun drill, the hoverbike chase and the endgame.
#
# Escape sequence: `pull switch` in Corridor 59Q turns the corridor lights on
# (global switch, not room-local) and opens the Mutilated Corpse; `search
# corpse` + `take lockpick` gets the lockpick used two rooms later. In
# Corridor 60Q, `press panel` (alias for the Door Switch1/"Lock Panel"
# object) burns the lockpick to unlock the Steel Door north into Corridor
# Intersection R, the hub room.
#
# From the hub: east to Utility Hall, `vandalize machine` -> disambiguation
# menu (answer bare `1`, any of the six identical "Heavy Machine" objects
# works) -> `take battery` -> `use battery` unconditionally grants a stun rod
# (the Conduction Rod some other machines drop is NOT required -- Battery1's
# `use` script fires regardless of whether it's held). Back west to the hub,
# north to the Guard Break Room: `interact warson` (with the stun rod
# already held, or it is an instant, unavoidable death -- "he's armed...
# shoots you") kills the Warson guard; `search dead guard` auto-grants a
# second lockpick (no `take` needed) and reveals a key card; `take key
# card`, `pick lock gunsafe`, `search gunsafe`, `take revolver`. South back
# to the hub, `swipe card panel` (the keycard-door object, alias "Reinforced
# Door Panel") unlocks the west Reinforced Door into Command Room1 ("A
# Command Room").
#
# The boss fight: `shoot kokouson` stuns Kokouson and opens `boss1` (the
# story flag that gates the rest of the level); the guard then knifes you
# for free. `shoot guard` kills him and is IMMEDIATELY followed by `e` back
# to the hub with ZERO extra commands in between -- Command Room1's own
# `description` script unconditionally re-checks `Got(Revolver1)` (false the
# instant the fight ends, since the revolver is swapped for a dummy prop)
# and kills the player outright on any further turn spent in the room,
# including a bare `look`.
#
# REAL-TIME CHASE (why this script needs `#!clock=5`). Corridor Intersection
# R's `description` script, once `boss1.isopen` is true, calls
# `EnableTimer(Kokouson Chase)`, a 25-second RECURRING authored `<timer>`
# (`msg("Kokouson enters the room and stabs you!"); DecreaseHealth(107)`)
# meant to chase the player out through the Pressure Room before it next
# fires; 107 damage is unsurvivable outright (`game.pov.health` is created
# at a hardcoded max of 100, source line ~24534), so the only way through is
# speed. Under the default DrainTimers model the chase never got its 25
# seconds: entering the hub also leaves ambient one-shot `SetTimeout` chains
# pending (Corridor 60Q's and Utility Hall's `RandomChance`-gated
# `firstenter` spawns, the cell's 15+45+1+60s intro chain), and the drain
# that waits those out ticks to the EARLIEST enabled trigger -- the chase --
# firing it twice inside the very `e` that enabled it. This script's golden
# was that forced death until 2026-09-25. On the typing clock the escape is
# `e` (Utility Hall; the exit script re-enables the chase, harmless), `e`
# (the now-unlocked Pressure Room Door into "Transition Room S", which opens
# `lockdeath1`), `pull switch` (closes lockdeath1, DisableTimer(Kokouson
# Chase), unlocks the Heavy Titanium Door) -- 15 seconds against a
# 25-second interval -- then `e` into the Cavern Network.
#
# Act 2, beyond the Cavern Network (all on the typing clock):
#  * Garbage Trench: `grab expired rations` (four +20 HP heals, kept for the
#    Devson fight), `take empty fuel can`; the ruined xport menu (`3`) and
#    the box of junk menu (`12` = the matches) answer by object order.
#  * Sewer: `damage sewage pipe` x2, `hide behind rubble` and wait out the
#    patrol (z x4), then on east; zoning out drops you back in the Trench.
#  * Grand Admiral: `eat fresh ration` / `eat canned veggies` (+`1`
#    disambiguation answers), key card, then the gun drill: `fire west gun
#    controls` / `fire east gun controls` timed with z's, then `ride
#    hoverbike`.
#  * Hoverbike chase: each rider's approach is a RandomChance draw, so the
#    answers are what this exact command history produces: "flies in front"
#    -> `shoot hoverbike controls`, "right side" -> `ram right ...`, "on your
#    left" -> `ram left ...`, otherwise `n`, until "You escape the valley".
#  * Cell escape: smash the toilet, `loot dode`, `wedge cracked wall`, then
#    `wiggle pipe cracked wall` until it breaks (also RandomChance-gated).
#  * Kokouson's wing: shower disguise, the inquire-kokouson dialogue, mandela
#    serum, severed hand, Warson IT admin, `drench computer` + `override all
#    locks computer`, the note + bomb, the bookshelf fire (douse + ignite),
#    `arena lock computer`; the four expired rations bring HP to 100 (each
#    heals on a timer ~3 commands later, hence the `z z z` gaps).
#  * Devson fight (RandomChance per swing): "dagger slips" -> `use dagger
#    devson`, "stumbles"/"evades your swing" -> `strike devson`, else `dodge
#    devson`. From less than full HP he wins.
#  * Endgame: combination lock 4 / 5 / 22 (each a `get input` answer on the
#    next line), stairs, `unlock generator`, `vandalize generator`, then 14
#    z's for the closing timers (13 is one short) and `sign recruitment
#    document`.
# Because the chase/wiggle/fight answers depend on the RNG history, ANY edit
# earlier in this file can reshuffle them -- re-derive the later answers
# against the oracle after touching it.
# `take all from cooking supplies` is a no-op ("I can't see that") kept because
# it spends 5 clock-seconds the later timing depends on. errors=1, steps=466.
#
e
pull switch
search corpse
take lockpick
n
press panel
n
e
vandalize machine
1
take battery
use battery
w
n
interact warson
search dead guard
take key card
pick lock gunsafe
search gunsafe
take revolver
take nutrient pack
1
take nutrient pack
s
swipe card panel
w
shoot kokouson
shoot guard
e
e
e
pull switch
e
eat nutrient pack
1
ne
ne
n
take chisel
take chargetape
s
sw
sw
se
inspect computer room door
use chargetape computer room door
sw
search desk
search grenade box
ne
take bomb
1
take bomb
sw
create time bomb tinker kit
create grenade tinker kit
interact computer
Deimos
interact computer
ne
nw
ne
throw time bomb
ne
e
in
throw grenade tank
e
pull switch
o
n
grab can opener
search cooking supplies
take all from cooking supplies
w
hide behind large crate
hide behind large crate
2
hide behind large crate
3
eat nutrient pack
heal bandage
heal bandage
heal bandage
heal bandage
z
z
z
z
z
z
z
z
z
search drunk guard
e
take canned fruit
take canned veggies
1
take canned veggies
1
take canned veggies
search weapon crate
search battery box
drop it battery
search battery box
drop it battery
search battery box
drop it battery
search battery box
drop it battery
search battery box
drop it battery
search battery box
e
grab flashlight
take wrapped sandwich
load battery flashlight
search battery box
drop it battery
search battery box
drop it battery
search battery box
build insulated blade
w
eat canned fruit
eat canned veggies
1
search supply crate
attack mushroom
attack mushroom
attack mushroom
attack mushroom
search mushroom corpse
loot odd key
loot cooked mushroom cap
eat wrapped sandwich
e
take heavy item
w
s
smash water pipe
Yes
n
e
w
Yes
s
z
z
z
z
switch flashlight
s
peek cell 1s
peek cell 2s
peek cell 3s
z
stab ghoul
search ghoul corpse
s
e
fight ghoul
s
s
e
s
search medical supply box
take heavy bandage
wash hands sink
n
w
n
search puddle
s
unlock heavy tech door
s
search dead warden
override locks bracelet
n
e
se
switch flashlight
heal heavy bandage
eat cooked mushroom cap
eat warden's lunch
e
se
attempt lock heavy metal cell
attempt lock heavy metal cell
unlock heavy metal cell
s
sleep on mattress
inspect mattress
bend spring
use lockpick lockpick
e
speak with southern cell
w
reach in toilet
e
speak with southern cell
s
talk with simple cage
d
cut metal wall
e
e
u
n
z
z
z
z
z
z
break rusty rail
d
search warden's desk
e
shoot lady bellonadaughter
e
z
z
z
speak with lady bellonadaughter
n
search fuel rack
take ep-2
take ep-3
add ep2 mamba
grab key key rack
add ep3 xport #1
enter vehicle xport #1
drive xport dash
drive xport dash
shoot rocket xport dash
thrust xport dash
thrust xport dash
pull up xport dash
thrust xport dash
in
grab expired rations
o
take empty fuel can
w
nw
inspect ruined xport
3
se
e
ne
search box of junk
12
sw
damage sewage pipe
damage sewage pipe
e
look at sewage channels
e
hide behind rubble
z
z
z
z
leave rubble
e
n
e
e
e
n
u
u
grab fresh rations
eat fresh ration
1
eat fresh ration
1
eat fresh ration
1
eat fresh ration
1
eat fresh ration
1
eat fresh ration
eat canned veggies
1
eat canned veggies
take key card
n
z
z
z
fire west gun controls
z
z
fire east gun controls
u
e
e
ride hoverbike
n
n
n
shoot hoverbike controls
n
n
n
shoot hoverbike controls
n
n
n
ram left hoverbike controls
n
n
n
ram left hoverbike controls
n
n
n
n
look at cell door
smash toilet
d
u
2
inspect toilet
loot dode
wedge cracked wall
wiggle pipe cracked wall
wiggle pipe cracked wall
wiggle pipe cracked wall
wiggle pipe cracked wall
wiggle pipe cracked wall
n
w
s
inspect corpse
n
look at cell 11q
s
take shower shower
change clothes shower
s
e
e
e
s
inquire kokouson
z
inquire kokouson
z
inquire kokouson
inquire kokouson
z
z
z
z
inquire kokouson
z
inquire kokouson
z
take mandela serum
look at counter
take severed hand
eat severed hand
n
w
w
n
attack warson it admin
inspect computer
drench computer
override all locks computer
s
brown button elevator controls
n
e
look at desk
read note
inspect bomb
w
n
pull switch
search cushion leather armchair
2
take vodka
douse bookshelf
ignite bookshelf
z
z
z
z
e
e
n
w
push glass chest
arena lock computer
eat expired ration
1
z
z
z
eat expired ration
1
z
z
z
eat expired ration
1
z
z
z
eat expired ration
z
z
z
e
n
n
z
accept challenge devson
dodge devson
strike devson
strike devson
strike devson
dodge devson
dodge devson
dodge devson
dodge devson
use dagger devson
loot devson
s
e
activate elevator controls
n
enter combination combination lock
4
enter combination combination lock
5
enter combination combination lock
22
n
climb up stairs
climb up stairs
n
n
unlock generator
vandalize generator
z
z
z
z
z
z
z
z
z
z
z
z
z
z
sign recruitment document
