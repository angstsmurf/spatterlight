# The Eye of Mandival (Father Thyme, 2014; v2.0 2023, ASL 580) — 49 rooms, 444
# objects. No published walkthrough exists anywhere; this 140-step script was
# derived from the game source and plays essentially the whole game. It reaches
# the single BEST ending: return to the Palace carrying the Eye AND the cook's
# button device, so Prang (not King Mandival) meets you in the throne room —
# "THE TOP RESULT IN THIS GAME". Coming home with the Eye but Prang's original
# button is the lesser ending; coming home empty-handed is the failure ending.
# Everything else that calls do_finish is a death.
#
# The win test is do_finish() with game.pov.parent = Palace; game.protected is
# false by default, so every other do_finish really does end the game.
#
# Route notes, all verified against the source:
#  - The whole game is one long chain and the last link is a WORD. The sentry at
#    the gate wants a password; the barman's hint (bought for ten grozzers) is
#    that it is an anagram of a ship's name. Four ships are nameable: WOLF (the
#    pirate ship), GOAT (on the coin), STOAT (the wreck, readable only with the
#    telescope) and STAR (the ship in a bottle, readable only with the witch's
#    magnifying glass). FLOW, TOGA and TOAST are all instant deaths at the gate;
#    RATS is the password. Hence the telescope must be TRADED to the witch — you
#    cannot keep it and still read the bottle.
#  - Dependency chain: enter the cage (that is what makes the dog and the barn
#    hole exist) -> x hole -> breastplate -> trade it to the swordmaker for a
#    blunt sword -> sharpen it on the flat rock in the forest maze -> cut the
#    dead shark open -> hook; rope from the gravedigger's shed; hook+rope ->
#    throw over the oak branch -> lucky charm from the jackdaws' nest -> beat the
#    one-eyed pirate at darts -> telescope -> witch -> magnifying glass -> STAR.
#  - x wolfhound TWICE: once in the barn (he retreats west, opening the gloomy
#    corner) and once in the corner (that is what yields the small key).
#  - The oak-tree clearing is a 4-room maze whose only way back is hidden: SEEK
#    in Thicker forest reveals it (the faded poster scrap in Even thicker forest
#    is the clue, "SEEK AND YE SHALL FIND").
#  - The witch's observatory is likewise hidden: the north exit from the oak tree
#    only appears after x map inside the pirate ship's brass-bound chest.
#  - Drinking the rum is deliberate, not a mistake: it is the only way into the
#    town jail, where standing on the bed yields the iron key to the artist's
#    cottage. Moving the painting there uncovers a grating, which is what lights
#    the beach cave so the skeleton's medallion becomes visible; the medallion is
#    the docker's pass into the dockyard, the warehouse job pays the ten grozzers
#    and the ten grozzers buy the barman's hint. Lie on the bed to get out.
#  - Naming: the ship in a bottle answers to "ship in a bottle", not "model ship";
#    the sea chest is "brass bound chest", not "brass chest".
#  - Do NOT x the crowd outside the dockyard gate: the pickpocket in it steals
#    the button device. (You can get it back from the one-legged drunk in the
#    courtyard, but there is no reason to lose it in the first place.)
#  - Visit the kitchen BEFORE the cook swaps your button: once Button.Prang is
#    set, the kitchen door is locked and the swap is unreachable.
#  - Endgame: take the mattress in the top-floor storeroom -> wing nut; oil it
#    with the oilcan that the "magic lamp" turns out to be; turn it and the
#    chandelier drops on Baron Waist. Only then is his office survivable. x the
#    glass fragments to pick up the Eye, then leave west — the guards arrive and
#    the button is the only way out.
#
# Fully deterministic: no RNG, no score and no timers anywhere on this path.
# state=Finished, errors=0, 140 steps.
x me
out
west
west
west
out
north
x hole
take steel breastplate
x wolfhound
west
x wolfhound
east
south
west
in
open chest
open tin box
take magic lamp
rub magic lamp
out
out
east
east
south
east
ask proprietor about sword
give steel breastplate to proprietor
out
west
west
west
in
take rope
out
east
east
east
north
east
southeast
east
sharpen sword
north
read paper scrap
southwest
seek
northwest
west
south
south
south
east
east
open shark
take hook
use rope on hook
west
west
north
north
north
east
throw hook tied to rope
up
x jackdaws nest
take lucky charm
down
west
south
south
south
south
west
north
open curtains
x shelf
take ship in a bottle
out
south
speak to large pirate
take telescope
down
down
open brass bound chest
x map
up
south
x wooden table
take bottle of rum
stand on bed
lie on bed
south
east
in
take painting
out
west
south
east
north
x skeleton
take medallion
south
west
north
west
west
speak to man behind a desk
out
east
north
west
south
give silver ten grozzer piece to thin man
north
east
north
east
north
north
give telescope to witch
x ship in a bottle
west
north
say rats
speak to one-legged man
north
west
ask cook about prang
up
up
take filthy mattress
oil steel wing nut
turn steel wing nut
down
east
x fragments of glass
west
out
