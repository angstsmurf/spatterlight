# The Legend of the Secret of the Smelly, Stinky Fish (ASL v580); no published
# walkthrough exists -- derived entirely from source (game.aslx, 10587 lines).
# An RPG-combat dungeon crawl through ~93 procedurally-connected cave rooms
# (Cave 1 through Cave 6, each ending in a shop) to the Underground River Bay,
# where the sole win is defeating "Giant Fish Mom" / spawn-alias "Unknown
# Giant Monster" (130 HP) -- her `changedhitpoints` handler, once hitpoints<2,
# sets flavour text, calls `love` (level-up) and `EnableTimer("End game")`, a
# timer whose script is a bare `finish`. Combat setup: `take spade` (its
# display alias is literally "Stinky Fish" -- the game's title reveal, matched
# by `<alt>` "spade"/"Spade") and `take potion bag` (its `<ontake>` script is
# what calls `UnlockExit` on the only exit out of the start room).
#
# Engine bug discovered from source: the `attack` command's script has two
# branches keyed on `player.equipped = null` that are byte-identical --
# `DoAttack(player, player, object, false)` either way -- so `equip`ping a
# weapon has ZERO effect on melee damage; only `shoot` (which needs a bought
# firearm + ammo) uses `player.equipped`'s stats. `equip spade` is issued
# below purely for realism/documentation; it does nothing mechanically.
# `DoAttack`'s formula: roll = d20 + attacker.attack - target.defence; roll>15
# = critical (x2 damage), roll>5 = hit, else miss; damage = base *
# (100-armour)/100, floored to >=1. Player base stats: hitpoints/max=110,
# damage=3, attack=1, defence=0, armour=0; the `love` function raises
# level/defence/attack/damage/hitpoints/max by +1/+1/+1/+1/+5/+5 each time
# player.exp crosses level*310+100, applied automatically after most kills --
# so later fights resolve in far fewer hits than early ones.
#
# `game.lock` mechanic: nearly every room's `<enter>` script spawns one or
# more monsters via SpawnTroll/SpawnDryad/SpawnUnicorn/SpawnOrc/SpawnOgre and
# sets `game.lock`, which `Lock1`/`Lock2` use to lock/unlock a global,
# map-spanning list of two-letter exit names -- meaning any single live
# monster ANYWHERE blocks ALL movement EVERYWHERE until it dies (its
# `changedhitpoints` handler clears the lock and calls `RemoveObject`).
# Spawned aliases are deterministic, not random (each Spawn* function's name
# pool is a single-element list): Troll="blue troll", Dryad="dryad guardian",
# Unicorn="unicorn", Orc="orc", Ogre="ogre", FishMom="unknown giant monster"
# (only renamed to "Giant Fish Mom" in her own death branch, after which any
# further "attack unknown giant monster" correctly fails with "I can't see
# that." -- harmless, does not affect errors=0).
#
# Disambiguation footgun: several rooms spawn TWO monsters sharing one alias
# (e.g. "Room 6" calls SpawnTroll(this) twice). The FIRST "attack <alias>" in
# such a room raises "Please choose which '<alias>' you mean: 1: ... 2: ...";
# answering "1" resolves it and completes that combat round normally. Once
# only one instance remains, the same "attack <alias>" resolves directly with
# no menu, and a stray leftover "1" answer just costs a free, harmless
# "I don't understand your command." turn (159 such no-ops in this script).
# So the uniform-safe pattern used throughout multi-spawn rooms is
# "attack <alias>" immediately followed by "1", repeated -- correct whether
# or not a menu is actually showing that round.
#
# All attack counts below are deliberately generous, fixed overkill budgets
# per monster tier (10 for Troll/Dryad/Unicorn, 16 for Orc, 20 for Ogre, 30
# for the final Fish Mom fight) rather than tuned to the exact empirical kill
# turn -- once a monster dies, further "attack <alias>" against it just prints
# "I can't see that." (468 occurrences here), which costs a turn but is not an
# error. Free full heals: every "R Shop" room (one per cave, all on the
# shortest path) and "Room 7" contain a "Spring Bath" object; `rest spring
# bath` fully restores HP for free (`Rest Bath` command, pattern
# "Rest #object#") -- used after every cave's gauntlet instead of buying
# Potions/Hyper Potions (100g/200g), which the free baths make unnecessary
# despite gold accumulating from kills. Lowest HP reached in this run: 16
# (well before a bath stop); player never dies.
#
# Real-time-timer footgun: the "End game" timer (`<interval>6</interval>`) is
# NOT turn-based -- this v580/legacy-V4 engine drives `<timer>` elements off
# real elapsed seconds (`WorldModel.RequestNextTimerTick`/`Tick(seconds)`),
# and the harness's automatic SetTimeout-draining only services timers named
# "timeout*". Any number of `wait` commands after Fish Mom dies (tested up to
# 40) never fires it. The fix is the harness's explicit `tick:N` script
# directive (`world.Tick(N)`, advancing the game clock by N seconds and
# firing whatever authored timer comes due) -- one `tick:10` immediately after
# the Fish Mom attack budget reliably fires "End game" -> `finish`.
#
# Route (BFS-shortest over the room/exit graph parsed from source, skipping
# several dead-end side branches not needed for the shortest path): room ->
# room 2 -> Room 3..7 (Cave 1, mostly single Trolls, one double) -> Cave2 ->
# Cave 234 -> Cave r 3..Cave 2 r 5 -> Cave 2 R Shop (bath) -> Cave 3 -> Cave
# 32 -> Cave 3 r 3..r 10 (Unicorns/Dryad Guardians/Trolls, several doubles and
# one triple) -> Cave 3 r Shop (bath) -> Cave 4 r 1..7 (no spawns) -> Cave 4 r
# 8..23 (Orcs, one double) -> Cave 4 r Shop (bath) -> Cave 5 R 1..11 (Orcs, one
# double) -> Cave 5 r Shop (bath) -> Cave 6 r 1..15 (Ogres) -> Cave 6 r Shop
# (bath) -> Underground River Bay (Fish Mom). errors=0, steps=1021.
take spade
equip spade
take potion bag
north
north
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
north
north
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
north
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
north
rest spring bath
north
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
east
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
rest spring bath
north
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
down
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
attack dryad guardian
down
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
down
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
down
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
attack unicorn
1
down
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
east
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack dryad guardian
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
attack blue troll
1
east
rest spring bath
south
south
south
south
south
south
south
south
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
attack blue troll
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
rest spring bath
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
down
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
attack orc
1
east
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
attack orc
east
rest spring bath
east
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
down
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
down
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
down
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
down
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
down
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
east
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
east
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
east
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
attack ogre
east
rest spring bath
east
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
attack unknown giant monster
tick:10
