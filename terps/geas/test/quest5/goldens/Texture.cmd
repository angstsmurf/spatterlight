# Texture — Ralf Thissen, 2015 (ASL 550)
# No published walkthrough exists — derived from game source (game.aslx).
# Adult horror story. Ending: `use emblem on small hole` on the first floor
# moves the player to the "forest ending" room, whose description (arrested
# for the murder of your wife) calls finish after an auto-continued wait.
# Chain: key under the door mat opens the front door; bloody kitchen knife
# on the toilet seat; the Anne Frank book-lever opens the secret room; the
# knife held to the attic mirror crosses into the mirror world (knife
# becomes clean); other side: power supply (kitchen cabinet) revives the
# laptop, whose police article ("Woman found dead in forest") sets the flag
# that arms the painting; knife-on-mirror returns; `x painting` now triggers
# the flashback to the bedroom where the wife is packing; `kill wife` (the
# mandatory scene, printed by the game) yields the emblem in the blood-
# soaked bedroom; the emblem in the painting's hole ends the story. The
# optional explicit scene is NOT taken. No score/RNG/timers; heavy
# ClearScreen/colour theatrics are display-only.
n
open gate
n
move door mat
take key
use key on front door
n
up
e
e
open curtain
x bath
x toilet
take knife
w
w
w
x bookshelf
take book
n
up
x mirror
use knife on mirror
down
s
e
down
w
open cabinet
take power supply
e
up
w
use power supply on laptop
turn on laptop
x screen
n
up
use knife on mirror
down
s
e
x painting
x wife
kill wife
take emblem
w
use emblem on small hole
