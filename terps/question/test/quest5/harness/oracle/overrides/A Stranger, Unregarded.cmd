#!clock=5
# A Stranger, Unregarded (Robin Craig, 2015, Quest 5.6, ASL 550) -- a full WIN
# (the better ending). Shipped as `Stranger.quest`, renamed here to its title. By
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
# CLOSET (Part 5): `close closet` before `lock closet` -- Core's lock on an
# open container is refused, and the crossbow must stay safely stashed.
#
# BACK HALF (Parts 7-11), on the typing clock. Every BlockInput + nested
# SetTimeout cutscene needs one `tick:5` line per nesting level before the
# next typed command, or that command is swallowed by BlockInput:
#   Vampire: `tick:5` after the coffin room entry and x3 after the ruby (the
#     vampire's rise); the sapling grows over 2+2 ticks (`water sapling`,
#     `pee on tree`). The lamp smashed at the portcullis; a second lamp lies in
#     the vampire's lair (`take lamp` / `x lamp`, lit for the cellar).
#   Siren / werewolf: earplugs, `kill siren with sword`, the silver cone from
#     the dead harpy, then back through Egypt to the closet for the crossbow
#     (`attach silver cone` makes the silver arrow) -> crystal amulet + `say
#     jyotiH` (permanent light; the lamp goes off and is saved for the Djinn).
#   `throw rope ladder` at Harpy1 on the way to the Wizard: the only way back
#     out of Wizard56 is north via Harpy17, which needs the ladder down (there
#     is no west exit from Wizard56, despite the derivation's Wizard guide).
#   The amethyst detour (after the ladder, dropping the crossbow: e e e n n n
#     to Harpy10, `take amethysts`) feeds the idol in the Demonlord's realm.
#   The cube maze: an un-reversed held bane makes every move random
#     (HandleMoving), so `cast SVREE'LAR'SPO-LELF` x3 right after `take bane`
#     (the first two casts are only "tries"). The spell toggles `.reversed` on
#     EVERY held item -- sword, armour and sack cost -2 health each while
#     reversed and the amulet's light goes weak -- so later the bane is dropped
#     before each cast (`drop bane` / cast / `take bane`) to un-reverse the rest
#     while keeping the bane guided.
#   Dragon: `light lamp` before the nomagic Dragon7 (the amulet fails there);
#     `wave staff` frees the Hellbeast, which is led through the lair (it eats
#     the dragon) into Dragon7 (where it dissolves). `x chest`, `put prism in
#     hole` -> sceptre. Guardroom (diadem worn, demon asleep): `x demon`
#     reveals the caught rock lemur and `kill demon` frees it to Ender10 (a new
#     demon appears and sleeps too). TreasureRoom: cast / `drop bane` / cast
#     leaves the bane there UN-reversed with everything held back to normal --
#     the bane's confusion is what makes the Demonlord's opening fireball miss.
#     Djinn: `wave sceptre` x2, `x urn` (the wax is not in scope until then),
#     lamp on for `melt wax`, `rub urn`, three ticks -> djinned sword. Orb.
#   Endgame: the warrior is dead (Ender5 kills her on first entry), so ONE
#     `kill ogre` at Ender29 + 5 ticks gets you knocked into Ender10 (extra
#     kills during the BlockInput only cost health). `x animal` there: the
#     lemur leads e e s s s s s s to its hidden SecretExit (Ender18 -> Ender24)
#     and west to the lifewater pool (`drink liquid`: full heal). Fetch the
#     warrior's body from Ender5 (`take warrior`; -4 health/turn while carried,
#     fine at full health), carry it back and `drop warrior` in the pool: she
#     revives and follows (+100; two ticks). The amulet lights the caves.
#     Shard from Ender21, then up to Ender1 and on to Ender29: `kill ogre` x4
#     with her drawing the other ogre, two ticks, `n` under the falling door.
#   DemonlordRealm: `show mirror to basilisk` / `take basilisk`; `put
#     amethysts in idol` / `rub idol` (+2 ticks); `show basilisk to ogre` (+2);
#     `take rocks`, `throw rocks at hydra` x3 (+2: the dart trap kills it);
#     throne room (+4 ticks for the missed fireball), `wave sword` (giant),
#     `throw shard at nimbus`, `kill Demonlord` x5; flee s e s s e e s s and
#     `break door` x4 -> the better ending, waking by the pool.
#
# ENDS: state=Finished, "Congratulations! You have defeated the enemy, and
# survived to tell the tale!", score 4185/5000, steps=820, errors=0,
# deterministic.
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
close closet
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
n
break basalt
break basalt
break basalt
s
take stone
n
n
x corpse
take crucifix
take glass amulet
sharpen crucifix
wear glass amulet
n
tick:5
throw stone
open coffin
use crucifix
n
take statuette
w
take lamp
x lamp
w
s
e
take ruby
tick:5
tick:5
tick:5
light lamp
x skeleton
take rusty key
break door
e
sheath sword
take bucket
s
drink water
w
down
e
e
fill bucket
w
w
s
water sapling
tick:5
tick:5
pee on tree
tick:5
tick:5
drop bucket
up
take fruit
up
n
n
n
n
e
e
e
e
se
s
unlock gate with rusty key
s
e
d
e
u
ring bell
look
look
look
look
take paper
open small box
w
w
d
u
w
take topaz
u
u
d
e
n
n
n
wear earplugs
w
n
kill siren with sword
n
take arrow
d
w
take pearl
e
u
s
s
w
w
x dead harpy
take silver cone
drink water
e
s
s
s
cast uttishhThata
w
s
e
read inscription
memorise jyotiH
w
w
d
s
s
s
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
unlock closet with key
sheath sword
take crossbow
attach silver cone
4
use wooden arrow
e
open door
aim crossbow
e
fire crossbow
take arrow
s
s
use wooden arrow
x compartment
take crystal amulet
wear amulet
say jyotiH
turn off lamp
n
n
w
w
n
throw rope ladder
n
n
drop crossbow
e
e
e
n
n
n
take amethysts
s
s
s
w
w
w
unlock metal door
23
55
17
open metal door
w
w
w
in
memorise hither
down
take cube
look at cube
down
w
take bane
cast SVREE'LAR'SPO-LELF
cast SVREE'LAR'SPO-LELF
cast SVREE'LAR'SPO-LELF
up
w
e
up
take golden key
take nail
down
n
n
s
e
unlock stone door with nail
e
x bench
take metal prism
look at cabinet
open hatch
put golden key in hatch
close hatch
push hatch
open hatch
take iron key
w
w
n
n
s
s
take diadem
e
e
n
look at spire
look at puzzle
4
read books
read books
read books
push button
1
look at puzzle
4
give cylinder to wizard
take staff
drop bane
cast SVREE'LAR'SPO-LELF
take bane
n
n
nw
w
up
up
n
n
w
w
s
unlock gilded gate with iron key
s
s
w
w
n
n
n
unlock iron door with sword
open iron door
light lamp
n
w
take star
out
out
s
s
s
s
s
w
push red button
w
s
e
take mirror
attach star to diadem
nw
n
e
e
n
n
w
w
w
n
w
n
n
n
wave staff
s
s
s
e
s
e
e
e
s
s
w
w
w
w
s
s
e
e
n
n
n
e
e
n
n
n
n
n
n
out
turn off lamp
s
s
s
s
s
w
w
w
w
w
w
s
s
x chest
put prism in hole
take sceptre
n
n
wear diadem
down
down
x demon
kill demon
in
cast SVREE'LAR'SPO-LELF
drop bane
cast SVREE'LAR'SPO-LELF
out
up
up
e
e
e
e
s
s
s
down
wave sceptre
wave sceptre
x urn
light lamp
melt wax
turn off lamp
rub urn
tick:5
tick:5
tick:5
up
n
n
n
e
e
n
n
w
w
w
n
w
n
n
n
n
wave sceptre
n
in
take orb
out
w
w
w
n
kill ogre
tick:5
tick:5
tick:5
tick:5
tick:5
x animal
e
e
s
s
s
s
s
s
w
drink liquid
n
n
w
e
out
w
w
take warrior
e
e
s
s
s
s
w
drop warrior
tick:5
tick:5
n
n
n
up
s
take shard
w
n
out
n
n
up
up
w
w
n
kill ogre
kill ogre
kill ogre
kill ogre
tick:5
tick:5
n
n
w
show mirror to basilisk
take basilisk
e
n
put amethysts in idol
rub idol
tick:5
tick:5
w
w
n
show basilisk to ogre
tick:5
tick:5
n
take rocks
throw rocks at hydra
throw rocks at hydra
throw rocks at hydra
tick:5
tick:5
w
n
tick:5
tick:5
tick:5
tick:5
wave sword
throw shard at nimbus
kill Demonlord
kill Demonlord
kill Demonlord
kill Demonlord
kill Demonlord
s
e
s
s
e
e
s
s
break door
break door
break door
break door
