#!errorlimit=200
#!clock=5
# Mt. Underlook: Trial of False Eyes (Quest 5, ASL v580, 25564-line game.aslx,
# 1294 objects); no published walkthrough exists -- derived entirely from
# source. BEST-REACHABLE SO FAR, NOT A WIN: the script clears the Act 1
# Kokouson chase (which needs the `#!clock=5` typing clock -- see REAL-TIME
# CHASE below) and stops alive in the Cavern Network; the vehicle chase and
# Act 2 that follow are reachable now and still owed.
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
# Script plays every Act 1 beat through the Kokouson boss fight and the
# chase and stops alive in the Cavern Network. errors=0, steps=30.
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
