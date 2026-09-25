# A Story of Salvation (Quest 5, ASL v550, 15657-line game.aslx); no published
# walkthrough exists anywhere -- derived entirely from source.
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
# nested `wait{...}` blocks do NOT pause for real player input in this
# engine (see ENGINE GAP below); no follow-up commands are needed or
# useful. `down` twice, `unlock trapdoor`/`open trapdoor` (trapdoor1, a
# second, different key from the lighthouse door), `down` into the
# catacombs.
#
# ENGINE GAP (this is where the script stops, at a forced, unavoidable
# death -- not a derivation mistake): the catacombs from catacombs9 onward
# are a real-time chase -- each room's `firstenter` sets a "following" flag
# and calls `SetTimeout(N){ if (flag) { death; finish } }` (60/45/45/30),
# meant to give an interactive player N real seconds to move to the next
# room before the pursuer catches them. QuestViva's headless oracle has no
# wall clock, so `Program.cs` reproduces the interactive timer tick
# deterministically by draining any pending self-destructing SetTimeout by
# its exact trigger delta immediately after each command settles (see main
# README, "Real-time timers: DrainTimers"). For a REVEAL timer (its
# documented purpose: Mouse Who Woke Up For Christmas, Escape From the
# Mechanical Bathhouse) this is exactly the intended faithful behaviour. But
# for a DANGER countdown like this one, draining the full 60-second delta
# the instant the room's own firstenter creates it means the death check
# fires atomically within the very same command that entered catacombs91,
# before any subsequent player command can ever be processed -- verified
# directly: the death message appears merged into the same output block as
# the room description, with no intervening "> " prompt. This is the exact
# phenomenon documented for The Encyclopedia of Elementals' Main Hall
# rescue window: there is provably no point in any script where a player
# choice could land inside the window, so the catacombs chase (and
# everything past it -- underground bunker, garden, Hardacre's lab,
# Somewhere, the ferris-wheel ending) is structurally unreachable from this
# oracle regardless of command choices, not a gap in this derivation.
#
# Script plays every reachable beat (asylum cell paper, nightfall4 trigger,
# ruined-house screwdriver, chapel paper/screwdriver/key puzzle, chapel-door
# ambush-avoidance, lighthouse key, dying writer's trapdoor key) and ends on
# the forced catacombs death. errors=0, steps=105, deterministic.
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
