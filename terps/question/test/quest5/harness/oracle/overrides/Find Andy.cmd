# Find Andy (Quest 5, ASL v580, game.aslx ~16100 lines decompiled); no
# published walkthrough exists anywhere -- derived entirely from source.
#
# Front half (Hallway through "give fudge to marjorie" at Fat Fighters):
# `give card to carol` (Office) fixes and relocates the wheelchair to Porch
# and makes the Fork (Kitchen) visible -- both must wait until after this.
# `use fork` at Pantry pries open a drawer for a `key`, kept for later (used
# on the Basement Chest, and given to Daffyd once the chest is open). Giving
# the chocolate cake to Maggie (Inside a Marquee) makes Carol sick, which
# reveals `fudge` at Office on next entry. `say sweeney minder` (Mansion
# Gate) unlocks the Mansion (Driveway). The Study `press thumbprint` /
# `538659` safe code yields the Certificate; giving it to Carol (Office)
# unlocks Office->east->Fat Fighters. Giving the fudge to Marjorie there
# yields a lipstick, unlocks Grove->up->Holly Bush ("nettleskey"), AND
# teleports Vicky from Quarry's "Inside a Hut" to... nowhere visible yet --
# she just becomes reachable there.
#
# ENGINE-GRAPH GOTCHA: Quarry's `up` exit to "Inside a Hut" carries
# `<locked/>` with NO `name=` attribute at all, so it can never be unlocked
# by any UnlockExit call in the source -- it is permanently, unconditionally
# blocked (message: "That way is too steep and slippery to climb up."). The
# only way into "Inside a Hut" is the long way around: Grove --up
# (nettleskey)--> Holly Bush --northwest--> Inside a Hut. "Inside a Hut"'s
# own `down` exit back to Quarry is a legitimate one-way chute slide. (Two
# other bare-`<locked/>`-no-`name=` exits exist elsewhere -- Long
# Gallery<->Conservatory in both directions -- and are likewise permanently
# sealed; Conservatory has nothing needed for this walkthrough.)
#
# Dock Leaves (Overgrown Meadow) sits nested inside a `hidechildren`
# "weeds" object and is NOT listed in the room description or directly
# gettable until `look weeds` reveals it first.
#
# Giving the Dock Leaves to Daffyd (Grove) relieves his nettle stings; he
# relocates himself to "a Shack" and unlocks Allotment->west->a Shack
# ("shackopen"). `give wheelchair to vicky` must happen at "Inside a Hut"
# (not Quarry) -- Vicky is there after the fudge/Marjorie trigger. It sends
# her sliding down the chute (dumping the wheelchair in Quarry below,
# collectible as `get wig` there) and makes `shirt`/`radio` (both physically
# located IN "Inside a Hut", not Quarry) gettable.
#
# Giving the radio to Ray (Reception) yields a Necklace. Giving the shirt to
# Daffyd (now inside "a Shack") yields a postcard. `unlock chest` (Basement,
# reached via a Shack->down) auto-adds the Film Script to inventory on open
# (container_lockable + autoopen, no separate `get` needed) -- the `key`
# from the Pantry drawer unlocks it automatically via scope-inventory match.
# `give key to daffyd` (back up in the Shack) requires the chest already
# open. Giving the Film Script to Dennis (Mansion Gate) yields a lightsaber.
#
# Rope/lightsaber triangle: `take rope` (Welly Throw Area), then `use rope`
# at The Old Well unlocks Old Well->down->Bottom of Well ("ropekey") --
# `use rope` is REQUIRED before the `down`, or it fails ("It is too slippy
# and dangerous to climb down."). `use lightsaber` at Bottom of Well unlocks
# Bottom of Well->east->Cave ("usetheforce1"). The Sewer/Cave/Bottom-of-Well
# triangle is the ONLY route to Ruins (Tunnel->north->Ruins is a dead-end
# spur off Sewer, itself only reachable via this triangle) -- Anne, and
# hence the dress, are unreachable any other way. Both `usetheforce1`
# (locked exit at Bottom of Well) and `usetheforce2` (locked exit at Sewer,
# ->south->Cave) RE-LOCK every time their owning room's `<enter>` script
# fires, i.e. every re-entry -- the return trip through Sewer needs a SECOND
# `use lightsaber` even though the forward Cave->north->Sewer leg needed
# none (that direction is unlocked outright).
#
# At Ruins, `give necklace to anne` reveals+drops a dress there; `get dress`
# then the three global `wear dress`/`wear lipstick`/`wear wig` commands
# (unrestricted by room, gated only on carrying the item) set Emily's
# "WigDressLipstick" state, required before `give postcard to emily`
# (Parlour) will unlock Parlour->down->Patio ("openpatio") instead of
# refusing.
#
# Patio win sequence is completely ungated by any of the room's own "look"
# flavour text: `look hedge` (reveals crevice) -> `look crevice` (reveals
# welly/`ball`) are flavour only; `get ball` unconditionally reveals feet
# and sets Andy found; `pull feet` sets Andy ready; `pull andy` (unrestricted
# beyond that) prints the full ending and finishes the game.
#
# errors=0, steps=167, deterministic, ends CONGRATULATIONS/completed.
west
southwest
south
east
give card to carol
west
north
northeast
get wheelchair
east
north
north
get fork
south
south
west
southwest
west
say jelly
east
south
south
south
east
east
north
give chocolate cake to maggie
south
west
west
north
north
west
say sweeney minder
west
west
up
north
press thumbprint
538659
south
west
north
north
use fork
south
south
east
down
east
east
east
east
give certificate to carol
get fudge
east
give fudge to marjorie
west
west
south
south
east
east
south
south
southeast
west
west
look weeds
get dock leaves
west
give dock leaves to daffyd
up
northwest
give wheelchair to vicky
get shirt
get radio
down
get wig
north
north
east
north
north
north
west
give radio to ray
east
south
south
south
east
east
south
south
southeast
west
west
west
south
west
give shirt to daffyd
down
unlock chest
up
give key to daffyd
east
north
up
northwest
down
north
north
east
north
north
west
give film script to dennis
east
south
south
east
north
take rope
south
east
south
south
southeast
use rope
down
use lightsaber
east
north
north
north
give necklace to anne
get dress
wear dress
wear lipstick
wear wig
south
south
use lightsaber
south
west
up
northwest
north
north
west
west
north
north
west
west
west
up
west
west
give postcard to emily
down
look hedge
look crevice
get ball
pull feet
pull andy
