# The Last Hero — Wayde Bairstow, 2014 (ASL 550)
# No published walkthrough exists — derived from game source (game.aslx).
# Three-act comedy quest (squire Dave vs Magnus the Great Dragon).
# BEST-EFFORT: the shipped game is UNWINNABLE. Every MoveObject into the
# challenge rooms misspells the room name ("The Warriors/Mages Challange"),
# an unknown object — so Duch never spawns, the mace/book/flame thrower,
# Bronze key, Glowing writing and Book of spells never appear, and Gimply's
# elevator to the dragon strictly requires the Titans hammer + Book of
# spells + Rogues cloak. Only the Rogues cloak is obtainable. This script
# drives every reachable beat cleanly and parks at the Room of Heros dead
# end wearing the cloak (2 script errors = the game's own bugs, well under
# the 20-error breaker; each is a valuable engine-parity case). The Warriors
# Challenge is never entered: its door locks behind you and the unlock is
# unreachable (Duch can't spawn) — a permanent soft-lock. Act 1: open window (the bee timers it enables
# are recurring = dormant headless), `squash bee`, search trash → bedroom
# key; cheese lures the giant rat out of the mouse hole → workshop key;
# Street shark extract revives Gregory (NOT consumed); craft gear (grindstone
# sharpens the blunt sword, hammer + work bench repair chain mail and the
# bucket helmet); gate needs helmet+mail worn & sword. The extract then buys
# the Merchant's Cute kitty cat, which claws the throne-room guard to death
# (he likes cats); King grants the quest. Sword chips a Yelping-tree branch;
# branch-fed Enchanted beaver chainsaws the tree into the Indestructible
# plank; plank levers the boulder onto the marauder blockade. Act 2: Talking
# door riddles (dave / snail / man / e as get-inputs); Duch dies by sword →
# resurrect → book distraction → mace to the skull → flame thrower (bronze
# key → Titans hammer); Rogues: push vase, steal the golden key, douse the
# torch BEFORE unlocking the golden chest (Rogues cloak); Mages: iceberg/
# fire/fish on left/middle/right pedestals, then `use lever` and the torch
# dousing both ERROR on the misspelled room and the puzzle dead-ends.
# All kill timers are recurring (dormant headless); no RNG.
open window
squash bee
search trash
unlock bedroom door
w
open cabinet
take wheel of cheese
use wheel of cheese on mouse hole
search mouse hole
e
s
unlock door
take street shark extract
e
n
use street shark extract on gregory
speak to gregory
s
w
search weapons pile
use blunt sword on grindstone
x armour stand
take torn chain mail
use torn chain mail on work bench
x tool rack
take hammer
use hammer on torn chain mail
wear chain mail
take battered helm
use battered helm on work bench
use hammer on battered helm
wear bucket helmet
e
unlock gate
w
give street shark extract to merchant
w
speak to gregory
speak to guard
give cute kitty cat to guard
w
speak to king
speak to gregory
e
e
e
s
use steel long sword on yelping tree
take tree branch
n
n
x pond
give tree branch to beaver
take enchanted beaver
s
s
use enchanted beaver on yelping tree
take indestructible plank
n
n
e
use indestructible plank on boulder
w
s
e
e
speak to gregory
n
speak to talking door
dave
snail
man
e
n
speak to gimply
e
push large vase
x pedestal
take golden key
switch off torch
unlock golden chest
open golden chest
take rogues cloak
wear rogues cloak
w
n
take element of ice
use element of ice on left pedestal
take element of fire
use element of fire on middle pedestal
take fish
use fish on right pedestal
use lever
switch off torch
s
speak to gimply
