# Mermaid Bay — Peter Edwards (also The Piskie, HMS Victory, Escape from
# the Forbidden City), ASL v550; no published walkthrough exists — derived
# entirely from game source (game.aslx, 418 objects). Cornish-holiday
# fantasy: a mermaid trapped in a rock pool needs a hand mirror-style comb
# delivered before she'll reward you with passage to an offshore island,
# where a wrecked ship's captain's cabin, a smuggler's den and a buried
# chest all feed into the final prop — a conch shell. Win: `blow conch`
# while standing on the small grassy plateau (its `<blow>` script only
# calls `finish` when `game.pov.parent = small grassy plateau`); this
# triggers the little-mermaid rescue ending, "THE END". 121 commands,
# errors=0, state=Finished.
#
# A death trap exists and is avoided: "further along the track" (east from
# "track") has an unconditional `beforeenter` script that kills the player
# outright — never visited by this route.
#
# Engine/game gotchas hit while deriving this:
#  - Alias overrides the internal object name for ALL parser matching, not
#    just display text. `octopus` (alias "tentacle") must be addressed as
#    "poke tentacle", never "poke octopus". Both hole objects share the
#    alias "hole": `hole1` (with the toad) and `hole2` (the smugglers'-den
#    entrance) must each be typed as plain "hole" — "look at hole1"/"use
#    ring on hole2" both silently fail with "I can't see that."
#  - `hidechildren` containers gate their children's name-resolution even
#    after the child's own `visible` property has been flipped true by a
#    custom script (e.g. via `MakeObjectVisible`) — an explicit
#    "look at <container>" is still required before the child becomes
#    nameable. Hit repeatedly: "look at rocks" before "look at small
#    ledge"/"take skeleton" (skeleton is revealed by hitting the limpets
#    with the stone); "look at oaks" then "look at hole" (hole1's alias)
#    before "take toad"; "look at cliff face" is NOT itself gated, but the
#    captain's cabin's box/paper need "look at opening"/"look at box"
#    reveals in sequence before "take paper" resolves.
#  - Two disambiguation menus appear and are answered with a bare number
#    on the following line: "use stone on limpets" -> 2 (the large green
#    stone, not the "stones" pile); "use brush on inky water" -> 2 (the
#    real "inky water" object, not "ink2" whose alias is also "ink").
#  - Decoy duplicate command pattern: a non-room-scoped global `<command>`
#    matches the literal text "dig sand in shadow" but is a complete
#    no-op/decoy everywhere (prints "There's no shadow here!" even at the
#    correct beach). The REAL dig logic is a room-scoped `<command>`
#    embedded inside the "sandy beach" object itself, matching the
#    shorter pattern "dig in shadow" (no "sand") — this is the one that
#    actually tracks the shadow position and reveals the buried chest
#    after enough attempts.
#  - The offshore island (wreck, cliff steps, smugglers den, sunny spots,
#    small wood, track) is reachable only via the cockle shell shortcut:
#    "drop cockle shell" + "enter cockle shell" on the sandy beach, and
#    the same pair again in reverse on the way back.
#  - Puzzle chain: assemble the spade (search rocks/seaweed for the head
#    and handle, put one in the other via a turnscript auto-combine) ->
#    crack limpets with a stone for a skeleton -> fill a bottle with sea
#    water -> ask the mermaid about being sad to trigger a wave that soaks
#    an octopus's cave -> poke the tentacle to retrieve a comb, fill the
#    bottle with the octopus's ink, dip a brush in the inky water -> give
#    the comb to the mermaid, who leaves a cockle shell -> use the shell
#    to reach the island -> jump the adder guarding a toad, catch the
#    toad and swap it for a ring hidden in nearby sprigs -> use the ring
#    on the hole (smugglers' den) -> press a chough carving to reveal a
#    recess holding a phial; break then warm the phial -> use the phial
#    to dissolve a padlock into the wreck -> in the captain's cabin, lie
#    on the cot three times (a dream sequence), then unlock/open a box
#    and brush the ink over a blank paper to reveal writing, then read it
#    -> back on the mainland, dig the sandy beach's shadow four times for
#    a buried chest, unlock/open it, search the feathers inside for the
#    conch -> return to the small grassy plateau and blow it for the win.
out
down
east
search sand
take brush
down
search seaweed
take spade head
up
west
west
search pebbles
take stone
west
search rocks
take spade handle
put spade handle in spade head
use stone on limpets
2
look at rocks
look at small ledge
take skeleton
down
search seaweed
take bottle
up
west
search plants
in
ask mermaid about sad
out
east
take object
poke tentacle
fill bottle
use brush on inky water
2
west
in
give comb to mermaid
out
east
down
drop cockle shell
enter cockle shell
southwest
up
up
west
jump over adder
west
look at oaks
look at hole
take toad
east
drop toad
look at sprigs
take ring
east
east
up
look at cliff face
use ring on hole
in
look at bricks
press chough
look at recess
take phial
break phial
warm phial
out
down
down
down
northeast
west
look at wreck
in
south
search barrels
take brass key
north
up
up
south
up
use phial on padlock
in
lie on cot
lie on cot
lie on cot
look at opening
unlock box
open box
look at box
take paper
use brush on paper
read paper
north
down
down
north
down
out
east
enter cockle shell
up
east
east
dig in shadow
dig in shadow
dig in shadow
dig in shadow
unlock chest
open chest
look at chest
take feathers
search feathers
take conch
up
blow conch
