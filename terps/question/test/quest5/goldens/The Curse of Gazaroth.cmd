# The Curse of Gazaroth (ASL v550, 13253-line game.aslx, 965 objects, 139
# top-level rooms). A published complete walkthrough exists at
# https://makemeanisland.wordpress.com/the-curse-of-gazaroth-complete-walkthrough/
# and its 18-step outline matches the actual room graph, but its prose
# ("hit troll with dagger until he dies") glosses over exact hit counts and
# several verb names that had to be reverse-engineered from source before a
# literal command script would actually reach `state=Finished`.
#
# Deterministic multi-hit combat: every monster/lock's verb handler is a
# `<VERB type="scriptdictionary"><item key="<Weapon>">` block using nested
# `firsttime { ... } otherwise { firsttime { ... } otherwise { ... } }` as a
# per-verb-per-weapon call counter -- it advances once per COMMAND regardless
# of the RandomChance-driven hit/miss flavour text, so the number of commands
# needed is fixed and deterministic (RNG only varies the flavour text and
# player damage taken, not turns-to-kill):
#   - Troll: `<hit><item key="Dagger">` has 7 nested firsttime levels before
#     the kill branch fires on the 8th call -> exactly 8x
#     "hit troll with dagger" (drops Blood Red Pearl, aliased "Red Pearl").
#   - Gazaroth: `<attack><item key="Gazaroths Sword">` has 9 nested levels,
#     kill fires on the 10th call -> exactly 10x "attack gazaroth with sword".
#   - Old Man Mooney: his `<hit>` is a plain `<script>` (not a
#     scriptdictionary), so it only fires on bare "hit mooney" -- "hit mooney
#     with dagger" answers "That doesn't work." His death branch drops the
#     Blood Red Pearl (if `Blood red pearl kit` is switched on, i.e. after the
#     pearl has already been given to him once) or a Rope otherwise, so he is
#     killed exactly twice across the walkthrough: once (implicitly, by never
#     killing him at all -- the pearl/rope exchange is via `give`, not
#     combat) and once for real near the castle.
#   - Locks (`hit lock with sword`) are plain single-call scripts, not
#     counters -- one hit each suffices, both at the mansion (Lock1, reached
#     via Master Bedroom -> north, gated on `Closet Door.isopen`) and at the
#     tunnel-maze escape (Storage Room's "Lock" object).
#
# Gloves: the "Gloves" object defines `<use>`/`<puton>` handlers that
# `SwitchOn(Gloves)`; there is no `<wear>` handler, so "wear gloves" answers
# "You can't wear them." -- "use gloves" is required. This matters because
# Orb of Frost's `<take>` checks `IsSwitchedOn(Gloves)`: without it, the 4th
# unprotected "take orb" attempt is FATAL ("Your stubborness overcomes your
# sense of reason... You have died." + finish). "use gloves" before the
# first "take orb" avoids ever hitting that trap.
#
# Bread/health bonus: Binny the Baker is asleep; taking or eating "bread"
# before waking her fails ("I can't see that.") because the Bread object
# doesn't exist yet -- her `<speak>` handler (triggered by "talk to binny")
# is what does `AddToInventory (Bread)`. "eat bread" then restores health
# lost in the troll fight.
#
# Fatigue timer: taking the flute on the island (Middle of Island's "Flute"
# object, `<ontake>`) starts a chain of ten nested `SetTurnTimeout` calls
# (9+8+7+6+5+4+3+2+1+1 = 46 turns total) ending in "You have lost." if not
# stopped. The escape route (island altar -> trapdoor -> tunnel maze back to
# Brown Ales Tavern) below takes well under 20 turns, so the countdown never
# gets past its second stage ("Your arms and legs feel like lead...").
#
# Orb/river: "throw orb into river" (matches `<throworbinto>` on the "River"
# object) unlocks the one-way pass east across the Fast Flowing River.
# Castle door: "give red pearl to door" (Black Door's `<give>` scriptdictionary
# keyed on "Blood Red Pearl") unlocks the castle entrance.
# Boat: must "get in boat" before "use oar"/"attach oar to boat" (its `<use>`
# script checks `game.pov.parent = Boat 1`), then "east" (a locked exit that
# `attachoarto`/`use` unlocks), then "get out of boat" on the island side.
# Altar: "move altar" (not "push", though both exist as independent counters)
# twice reveals the island trapdoor.
#
# Win: giving the flute (alt "Flute 2", picked up as "Flute of Fate") to the
# Flute Player in Brown Ales Tavern -- "give flute to player" -- triggers his
# `<give><item key="Flute 2">` handler, printing the ending poem and
# "You have won the game." + finish. This is the actual win; Gazaroth's own
# death ("Finnaly you strike a fatal blow...") only unlocks the tower/locket
# route, it does not end the game.
#
# Deterministic seed=1234 (QVH_SEED default) makes this exact command
# sequence reproduce identically every run despite the RandomChance() calls
# throughout combat.
north
north
northeast
east
north
north
northwest
north
in
ask ale about curse
ask ale about mountain
ask ale about gazaroth
out
north
west
northwest
west
in
ask pastor about curse
out
east
southeast
east
north
north
north
west
in
ask mooney about curse
out
east
south
south
south
west
northwest
north
northwest
north
east
in
take purse
open purse
take silver piece
out
west
south
southeast
south
southeast
east
northeast
in
give silver piece to joe
take gloves
out
southwest
north
north
north
north
north
northwest
northwest
northwest
northeast
southeast
down
hit troll with dagger
hit troll with dagger
hit troll with dagger
hit troll with dagger
hit troll with dagger
hit troll with dagger
hit troll with dagger
hit troll with dagger
take red pearl
up
northwest
southwest
south
south
southeast
southeast
south
in
give red pearl to mooney
out
east
south
south
south
west
northwest
north
in
talk to binny
eat bread
out
south
southeast
east
south
south
southeast
south
south
west
southwest
south
tie rope to beam
down
take key
up
north
northeast
east
in
west
unlock cellar door with key
open cellar door
down
open large chest
take sword
up
east
out
north
north
northwest
north
north
east
south
southeast
southwest
southeast
northeast
in
up
east
hit lock with sword
open closet door
north
open small chest
use gloves
take orb
south
west
down
out
southwest
northwest
northeast
northwest
north
west
north
north
north
east
east
east
northeast
northeast
throw orb into river
southwest
southwest
west
west
west
west
in
hit mooney
take red pearl
out
east
east
east
east
northeast
northeast
east
east
east
give red pearl to door
north
north
northeast
up
north
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
attack gazaroth with sword
northeast
take locket
southwest
south
down
southwest
south
south
west
west
west
southwest
southwest
west
west
west
south
south
south
west
southwest
in
give locket to jim
ask jim about oar
out
northeast
east
east
get in boat
attach oar to boat
east
get out of boat
east
take flute
move altar
move altar
open trapdoor
down
west
west
west
northwest
southwest
south
southwest
west
hit lock with sword
open trapdoor
up
south
out
east
southeast
east
south
in
give flute to player
