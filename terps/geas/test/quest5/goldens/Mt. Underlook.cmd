#!errorlimit=200
# Mt. Underlook: Trial of False Eyes (Quest 5, ASL v580, 25564-line game.aslx,
# 1294 objects); no published walkthrough exists -- derived entirely from
# source. THIS IS THE BEST-REACHABLE SCRIPT, NOT A WIN -- the game's own
# engine-vs-oracle mismatch (detailed below) forces death at the first
# chokepoint of the underground-prison Act 1, well before the vehicle-chase
# and Act 2 content that follows it.
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
# THE HUB IS UNREACHABLE-ALIVE FROM HERE -- CONFIRMED ENGINE GAP, NOT A
# DERIVATION GAP. Corridor Intersection R's `description` script, once
# `boss1.isopen` is true, calls `EnableTimer(Kokouson Chase)`, a 25-second
# RECURRING authored `<timer>` (`msg("Kokouson enters the room and stabs
# you!"); DecreaseHealth(107)`) meant to chase the player out through the
# Pressure Room before it next fires. 107 damage is unsurvivable outright:
# `game.pov.health` is created at a hardcoded max of 100 (source line
# ~24534), so even full health cannot absorb the hit -- this rules out any
# "minimize prior damage" strategy. But the timer does not even get a fair
# 25-second window: several rooms on the only route to the hub (Corridor
# 60Q's `firstenter`, Utility Hall's `firstenter`, both `RandomChance`-gated
# ambient encounter spawns) queue their own one-shot `SetTimeout(...)`
# chains, and the very first room's own intro schedules a 15+45+1+60s chain
# before the player ever leaves the cell. The oracle's `DrainTimers` (see
# harness/oracle/README.md's "Real-time timers" section) only *decides*
# whether to keep ticking by checking for still-pending "timeout*"-named
# timers, but each `Tick()` it does perform advances `game.timeelapsed` and
# fires EVERY due timer, named or not -- so any one of these unrelated
# ambient chains still pending when `EnableTimer(Kokouson Chase)` runs
# immediately satisfies the freshly-set trigger as a side effect. Five
# independent live tests (identical death point regardless of exact
# turn-count, including one with a confirmed genuinely turn-consuming filler
# inserted) rule out a turncount-parity theory -- the kill is a structural
# side effect of how the headless oracle drains real-time timers, and it
# fires on the very same command that opens `boss1`, before any escape
# command can ever be issued. This is the same phenomenon already documented
# for The Encyclopedia of Elementals' Main Hall rescue window and A Story of
# Salvation's catacombs chase.
#
# Script plays every reachable Act 1 beat up to and including the Kokouson
# boss fight and ends on the forced, unavoidable death. errors=0, steps=26.
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
