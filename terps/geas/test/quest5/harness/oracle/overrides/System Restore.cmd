# System Restore — by AvecPardon, 2012 (ASL 550, standard parser style, not
# gamebook; v1.3). No published walkthrough exists — derived from game source
# (game.aslx, 8318 lines, 205 top-level objects/349 total, an RPG-styled
# sci-fi mystery: a boy plays a "game" that turns out to be him remote-
# controlling a real android, Caleb, whose stolen "soul" he must recover
# while exploring a multi-floor corporate building via colour-coded
# keycards/card-readers, avoiding security guards, and managing an HP/mana/
# "Systems Damage" stat trio).
#
# THE GAME HAS NO WIN CONDITION IN THE SHIPPED SOURCE (confirmed via
# exhaustive `grep -n 'finish'` over the whole file): the ASL `finish`
# primitive is called in exactly THREE places, and none of them is a win —
# (1) declining the "Activate Remote Link?" prompt at the very start (a bad
# END screen: the player quits and the villain wins off-screen), (2) the
# `damage_check`/initial `player.damage_check` handlers' "80+ Systems
# Damage" game-over (Caleb's mind gets fried), and that is the entire set.
# There is no "you win"/"congratulations"/epilogue text anywhere in the
# file, no call that ever resolves the "find the soul, defeat Gatekeeper"
# plot the intro sets up, and three near-identical "the Elite" baton-shock
# capture scenes (each just resets the player back to the starting Aqua Lab
# via `frst_capture`/`health_check`) that read as a serial cliffhanger, not
# a climax. This is a corpus-parity case with WAKE (also `Running`, `game is
# UNWINNABLE`): the win path was simply never written, not merely bugged.
#
# Given that no ending exists to chase, this override does not attempt an
# exhaustive traversal of the building's ~20+ rooms/multiple floors and RPG
# combat system (auto-attacking security guards, four more colour-locked
# labs each needing their own keycard found elsewhere, an elevator/stairwell
# to at least three more floors). Instead it plays a clean, representative
# opening sequence and parks in a safe, no-encounter room, exactly matching
# the game's own stated logic for what counts as "safe" (Caleb: "Rooms
# should be safe, so long as there is no one already inside them").
#
# Route: intro (accept the Remote Link, name the player "Robin") -> Aqua Lab
# puzzle: `look at computer desk` reveals the terminal, `search computer
# desk` reveals the one unlocked drawer, `open drawer`/`take aqua keycard`/
# `close drawer` (closing it is a scored action), `look at computer
# terminal` reveals the connector's input pad, `use connector's input pad`
# is the real gameplay-unlock beat — it turns on the HP/mana/Systems Damage
# status trio and timers Caleb needed disabled-by-default logic is built
# around (`health_check`/`damage_check`) -> `use aqua keycard on card
# reader` unlocks the lab door -> `east` into Hallway A (Caleb's warning
# about wandering hallways: guards can ambush you there) -> `south` into
# Hallway B, where the one door with no card reader at all is the Lounge ->
# `west` into the Lounge (a safe room, disables the roaming `security`
# timer) -> `in` to the kitchen sub-area, a genuine plot beat where Caleb's
# fragmented memory reacts oddly to "kitchen"/"cooking" (foreshadowing his
# true, non-combat original function) -> `out`/`inventory` to confirm a
# clean, stable stopping point. errors=0, state=Running (the only reachable
# terminal states are the two bad-ending `finish` calls above; this script
# deliberately avoids both).
yes
Robin
look at computer desk
search computer desk
open drawer
take aqua keycard
close drawer
look at computer terminal
use connector's input pad
use aqua keycard on card reader
east
south
west
in
look
out
inventory
