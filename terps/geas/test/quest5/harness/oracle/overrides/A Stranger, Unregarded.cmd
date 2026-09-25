#!clock=5
# A Stranger, Unregarded (Robin Craig, 2015, Quest 5.6, ASL 550) -- best-reachable
# so far, NOT a win. Shipped as `Stranger.quest`, renamed here to its title. By
# far the largest game in the corpus: 350 rooms across ~20 regions (several
# mazes), hunger/thirst/health survival, a difficulty select, score /5000; the
# win is defeating the Demonlord. No published walkthrough exists anywhere --
# derived entirely from game.aslx (first known walkthrough for the game; the
# full 11-part human-readable derivation, with per-region source maps, lives in
# ~/Downloads/Quest 5 walkthroughs/Stranger-derivation/).
#
# `#!clock=5` (the typing clock: five seconds per typed command, nothing
# drained) is what lets this script exist at all -- see THE PORTCULLIS below.
#
# SETUP: sex `m`, name `Robin`, bravery `chicken` (game.safety=2: DeadMeat/
# FatalMove "silly mistake" deaths become health-5 warnings instead of instant
# deaths; health reaching 0 still kills even on Chicken, since game.onhealthzero
# is not gated by safety). Health, hunger and thirst tick every turn: wear the
# armour (+1 health/turn), drink at water sources, eat when low.
#
# ROUTE (Parts 1-6 of the derivation): Start (beach, marshland worm in the
# reeds, revive the Amazon warrior with `give orchid` / `give berries` / `kiss`,
# which opens the pyramid) -> Pyramid interior -> Troll & Bear (`give fish to
# cave bear`; the warrior is removed on Troll1 entry and rejoins in the
# endgame) -> Egyptian pyramid (`memorise` the hieroglyph words; closet key
# under the mat in Egypt46and47) -> Harpy domain (`ready crossbow` / `aim
# crossbow` / `fire crossbow` from Harpy1 -- never fight on the bridge) ->
# stash the crossbow in the Egypt closet (Harpy3's hole rejects it and the
# gnomes steal anything dropped; it is needed again for the werewolf) -> bell
# from the Harpy13 cupboard -> `ring bell` in Harpy6 raises the portcullis.
# Nouns come from ALIASES, not object names (`x bas-relief`, `cave bear`);
# Core `Ask` prompts are numbered text menus answered with a bare `1`/`2`.
#
# THE PORTCULLIS (why this script needs `#!clock=5`): UnderPortcullis ("About
# to Die", north of Harpy6) is the SOLE gateway to the back half of the game
# (Vampire -> Siren -> Wizard -> Dragon -> Endgame; Siren1->Vampire27 is only
# reachable from inside). Its `enter` script arms `SetTimeout(9){ if still
# here: bounce back to Harpy6 and -10 health on Chicken, `finish` (crushed) on
# Fox/Lion }` -- a reflex step: the player types `n`, `n` within nine real
# seconds. Under the default DrainTimers model (a player who waits out every
# pending SetTimeout before typing again) the bounce fired inside the very `n`
# that entered the room, every time, and the game was kept out of the corpus
# as oracle-unwinnable (2026-07-21). On the typing clock the second `n` lands
# at 5 s and the script walks through into Vampire1 (+20; the lamp smashes --
# the crystal amulet behind the post-Siren werewolf is its replacement).
#
# STOPS at Vampire1, the first room of the back half: state=Running, errors=0,
# steps=308, deterministic. Parts 7-11 of the derivation (Vampire, Siren, the
# werewolf back through Egypt, the Wizard's Rainbow maze and the Dragon, the
# Endgame and the Demonlord) are source-derived, not yet oracle-driven, and
# still owed.
m
Robin
chicken
stand up
n
take bottle
n
n
e
s
drink water
take berries
n
nw
w
nw
n
up
take orchid
down
ne
ne
x reeds
take worm
open bottle
put worm in bottle
close bottle
sw
sw
s
se
e
give orchid to warrior
give berries to warrior
kiss warrior
up
up
take dagger
down
put dagger in slot
in
s
w
w
n
e
take stool
e
take needle
w
w
s
e
e
n
n
w
stand on stool
up
take sack
take bronze key
open gate
e
unlock closet with needle
open closet
take armour
w
up
e
s
s
w
w
n
unlock gate with bronze key
open gate
n
take lamp
light lamp
wear lamp
n
1
1
n
w
n
open door
in
break statue
take sword
read book
read book
read book
read book
memorise SVREE'LAR'SPO-LELF
out
s
e
s
s
s
e
e
e
in
take twine
tie needle to twine
bend needle
out
w
w
w
s
e
e
n
down
down
down
w
s
up
s
w
w
kill wraith with sword
w
up
n
n
x bas-relief
x face
push button with sword
open safe
take amulet
e
n
down
w
up
n
w
w
w
n
w
n
w
take hemlock
e
s
s
w
s
s
break bottle
take worm
bait hook with worm
catch fish
wait
wait
wait
wait
wait
take fish
put hemlock in fish
n
n
e
n
e
s
e
e
e
s
down
give fish to cave bear
e
give amulet to bear
n
take bracelet
open gate
n
up
e
n
n
kill troll with sword
s
s
w
down
s
n
up
e
n
n
e
take opal
take sandwich
w
n
n
n
w
w
s
s
up
e
x ashes
take cylinder
w
down
n
n
e
e
e
n
up
e
e
down
w
n
n
e
e
sheath sword
take crossbow
w
w
n
open cupboard
take box
s
s
s
w
n
n
x skeleton
take arrow
s
s
s
s
s
open sarcophagus
x mummy
x ring
memorise FALNX
close sarcophagus
n
n
n
e
s
e
say FALNX
e
take emerald
w
w
n
e
up
w
w
down
s
w
n
ne
n
w
lift mat
take key
n
ready crossbow
aim crossbow
fire crossbow
n
n
s
s
s
unlock closet with key
open closet
put crossbow in closet
lock closet
n
n
n
e
e
e
e
open cupboard
take bell
w
w
ring bell
n
n
