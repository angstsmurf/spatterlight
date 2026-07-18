# Quest for loot and something else — Mugi4ok, 2016 (ASL 550)
# No published walkthrough exists — derived from game source (game.aslx).
# A birthday giveaway game: rooms print Steam-game links as "loot".
# Ending: the "strange place" south of the really white room (locked at
# start) asks for gratitude via two get-inputs — "thanks" then "yes" — and
# calls finish. It unlocks when both sides are complete: entering the dark
# `future` room sets eastComplete, and `use portal` at the end of the west
# walk sets westComplete (the portal also re-sets eastComplete — an author
# shortcut this script does not rely on: the east side is played properly).
# East chain: red scarf in noir hides the ridiculously huge key -> the
# freaking giant keyhole unlocks `present`; the time room hands over the
# numbers note; naval's device + note teleports to the witcher world (same
# device returns); spaceship -> space -> wasteland sheriff star -> a shady
# Wild-West stranger swaps it for the grey box medal -> the battlefield
# squad accepts you; warzone bike keys start the racing-track bike; Neo
# Tokyo's ship part repairs the ship (unlocking the outer ring) and the
# eavesdropped address note (from `look south` in noir) opens the Tokyo
# inn. Entering `future` via the space-door back way completes the east.
# No RNG/timers; DisplayHttpLink output is deterministic.
x switch
switch on switch
e
x freaking giant keyhole
n
w
x message board
w
x red scarf
look south
e
e
s
use ridiculously huge key on freaking giant keyhole
e
n
n
e
e
e
x animals
s
w
e
use spaceship
w
take sheriff star
n
take bike keys
n
take ship part
w
e
s
s
e
e
n
take coin
n
w
n
n
n
w
open grey box
w
n
x porthole
use device
use same device
s
e
e
s
e
n
use squad
n
e
use bike
w
s
s
w
w
w
w
w
w
n
n
n
n
w
w
w
w
w
use portal
s
thanks
yes
