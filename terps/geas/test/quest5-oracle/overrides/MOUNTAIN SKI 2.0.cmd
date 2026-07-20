# Mountain Ski 2.0 -- Patricia, 2024 (Quest 5.8.6836.13983, ASL 580) -- WINNING SCRIPT
#
# RESULT: state=Running at the credits, errors=0, 22 steps. There is NO `finish`
# in the game (Serpent's-Eye / spondre / Sword-and-Spell pattern): the Motel
# Lounge's `enter` script prints "THE END" + full CREDITS the instant you walk
# in while BOTH lost dogs are following you. Reaching that text IS the win; the
# engine stays Running because the room script never calls finish or quit.
#
# SOURCE: no published walkthrough exists -- the script was derived entirely
# from game.aslx. A cozy winter-holiday errand: your sister's motel friend
# ("Pete") lost his two dogs, and the whole game is befriending each with its
# correct treat and leading both back. Win check (Motel Lounge <enter>):
#   if (Sebastian.parent = game.pov and Rex.parent = game.pov) { ...THE END... }
#
# THE TWO DOGS + their treats (each treat is the ONLY thing that befriends its dog):
#  * Rex        -- grey husky, in the dark Cave. `use dog biscuit on rex`
#                  (dog biscuit is loose in the Guest Room; sister's prank).
#                  useon "Dog biscuit" sets flag friend + AddToInventory(Rex).
#  * Sebastian  -- white Great Pyrenees, Down the Hill. `use peanut butter on
#                  sebastian` (peanut butter is in the Lounge cupboard).
#                  useon "peanut butter" sets flag friend + AddToInventory.
#
# TWO SIDE-GATES, one per dog's location:
#  * The Lake path is blocked by a snow pile: `dig snow` while carrying the
#    shovel (Lounge cupboard) does UnlockExit(lakexit). The Cave hangs off the
#    Lake, so Rex is unreachable until you dig.
#  * The Mountains refuse entry without the skis (Guest Room), and the hill down
#    to Sebastian is the `HillDown` exit, auto-unlocked on entering the Mountains
#    while carrying skis.
#
# FOOTGUNS (author bugs left in, avoided here):
#  * `use skis` is DEAD: its script tests `skis.parent = game.pov.parent`, true
#    only if the skis are dropped on the ground in the Mountains. The working
#    descent is the exit `go down the hill`.
#  * The "go home" tell scripts (tell/tellto the dogs) reference a nonexistent
#    object `Lounge` and an undefined variable `white dog` and throw -- so the
#    dogs are led home simply by walking there while they follow (inventory).
#  * Navigation is `go`-prefixed BY ALIAS: `the alps`, `down the hill`, `cave`,
#    `lounge` are exit aliases -- a bare word ("the alps") is "I don't understand".
#  * The Cave is dark: `switch on torch` reveals Rex before you can befriend him.
#
go the alps
take skis
take dog biscuit
down
open cupboard
take shovel
take peanut butter
go outside
dig snow
go mountains
go down the hill
use peanut butter on sebastian
go mountains
go outside
go lake
go cave
take torch
switch on torch
use dog biscuit on rex
go lake
go outside
go lounge
