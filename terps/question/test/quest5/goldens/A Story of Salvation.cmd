#!clock=5
# A Story of Salvation (Quest 5, ASL v550, 15657-line game.aslx); no published
# walkthrough exists anywhere -- derived entirely from source.
#
# `#!clock=5`: the catacombs are a real-time chase (see REAL-TIME CHASE
# below), so this script runs on the typing clock -- five seconds per typed
# command, nothing drained -- instead of the default DrainTimers model.
#
# Day/night trigger chain (nightfall4 must fire before the ruined house's
# screwdriver becomes reachable): visit the asylum cell (picks up a piece of
# paper), return to the jetty via the full outbound route in reverse, then
# `wait` x4 at the jetty -- this auto-teleports the player to the gift shop
# with the torch lit and flips time-of-day flag "nightfall4" on.
#
# Gate/window bypass: `climb gateposts`/`climb gatepost` gets over locked
# gates in both directions; the ruined house's boarded window needs `pull
# sheeting` then `west` through the gap (not a normal door).
#
# `main hall` is a dead end without a detour: `open north door` then
# `north`/`east`/`southeast` through `another corridor`/`corridor2`/
# `corridor3` reaches the asylum cell -- a straight walk from main hall does
# not.
#
# Chapel-door puzzle: `use paper on chapel door` (slides it under the door),
# `use screwdriver on keyhole` (pushes the chapel key through onto the
# paper), `take paper`, `take chapel key`. VERB SYNTAX GOTCHA: this game's
# `unlock`/`lock` verbs have NO "with X" clause at all (`<pattern>unlock
# #object#</pattern>`, ditto lock) -- `unlock chapel door with chapel key`
# fails with a misleading "That doesn't work." (not an in-fiction refusal).
# The bare `unlock chapel door` succeeds; `container_lockable`'s own script
# checks scope inventory for the matching key automatically.
#
# Alias-collision death trap: `outside chapel`'s door and `chapel2`'s door
# are two DIFFERENT internal objects (`chapel door` / `chapel door1`) that
# share the alias "chapel door" -- always type the bare alias, never the
# literal "chapel door1" (that fails "I can't see that.", leaving the door
# NOT locked). After taking the chapel key and going north into chapel2,
# `lock chapel door` (from inside chapel2) arms the door against the
# old-woman ambush that triggers on re-entering chapel2 once the lighthouse
# key (chapel4's `rack of keys`) has been taken -- skip the lock and the
# ambush kills you. `unlock chapel door` again before leaving south.
#
# Lighthouse: `unlock door`/`open door` (door99, keyed to the lighthouse
# key) then north into the lighthouse, `up` twice to the top. `x slumped
# figure` (the dying writer) prints his ENTIRE multi-stage dialogue and
# hands over the trapdoor key in one synchronous command -- the source's
# nested `wait{...}` blocks are auto-continued by the harness (a `wait` is a
# keypress prompt, not a timer); no follow-up commands are needed or useful.
# `down` twice, `unlock trapdoor`/`open trapdoor` (trapdoor1, a second,
# different key from the lighthouse door), `down` into the catacombs.
#
# REAL-TIME CHASE (why this script needs `#!clock=5`): the catacombs from
# catacombs9 onward are a chase -- each room's `firstenter` sets a
# "following" flag and calls `SetTimeout(N){ if (flag) { death; finish } }`
# (60/45/45/30 seconds), the next room's entry clears the flag, and the
# player is meant to keep moving west. Under the default DrainTimers model
# (a player who waits out every pending SetTimeout before typing again) the
# 60-second countdown fires inside the very command that entered
# catacombs91 and the game ended there -- this script's golden was a forced
# death until 2026-09-25. On the typing clock the four `west`s cost 20
# seconds in total, each countdown finds its flag already cleared, and the
# player bars the bunker door behind them.
#
# Endgame: from the bunker `w` x2 to the garden, `s`, `w`, then `n` x4
# (crossroads, blighted path, woods, the ruined laboratory). Entering the lab
# plays Hardacre's monologue through nested `wait`s and drops you in
# "somewhere" with your eyes closed (inventory stripped); `open eyes` runs the
# ferris-wheel ending and `finish`. The game has only this one ending; the
# bullets in the east woods building and the locked north gate are red
# herrings.
#
# Script plays the whole game (asylum cell paper, nightfall4 trigger,
# ruined-house screwdriver, chapel paper/screwdriver/key puzzle, chapel-door
# ambush-avoidance, lighthouse key, dying writer's trapdoor key, the
# catacombs chase, Hardacre's lab) to THE END. errors=0, steps=118,
# deterministic.
north
north
west
west
north
north
northeast
east
north
north
north
north
climb gateposts
northeast
pull sheeting
west
west
west
west
open north door
north
east
southeast
take piece of paper
west
west
south
east
east
east
east
southwest
climb gatepost
south
south
south
south
west
southwest
south
south
east
east
south
wait
wait
wait
wait
north
west
west
north
north
northeast
east
north
north
southwest
x pile of straw
take screwdriver
northeast
south
south
west
north
north
east
use paper on chapel door
use screwdriver on keyhole
take paper
take chapel key
unlock chapel door
open chapel door
north
lock chapel door
north
x rack of keys
take lighthouse key
south
unlock chapel door
south
west
south
south
east
north
north
north
northeast
northeast
unlock door
open door
north
up
up
x slumped figure
down
down
unlock trapdoor
open trapdoor
down
southwest
southwest
west
west
west
west
west
west
w
w
s
w
n
n
n
n
open eyes
