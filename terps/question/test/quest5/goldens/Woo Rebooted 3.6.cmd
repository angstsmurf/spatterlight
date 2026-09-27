# Woo Rebooted ("Woo Rebooted 3.6.quest", Longshot & DavyB, 2016/v3.6 2024,
# ASL 580) - full winning walkthrough. Derived from source; no published
# walkthrough exists.
#
# You are Doctor Woo; the BARDIS (your space-time craft) has an empty fluid link
# and no toilet paper. WIN = fluid link full of mercury back in the control
# console + a toilet roll in the holder, then pull the handle twice. Around that
# sits a full Cluedo-style murder mystery: Dr Black is dead and Kojak will only
# name the killer once the notebook holds >= 32 clues (game.clues_required) AND
# the Secret Nuclear Bunker has been found.
#
# The game opens in five gated stages, each needing the previous one:
#   L2 beanstalk  - trade the can of baked beans to a recovered Jack
#   L3 east road  - a blessed iron nail (tweezers -> tyre, Rev Keen) to Monty Python
#   L4 Manor House- gas mask (Trumper's present) to walk through the cottage fumes
#   L5 mountains  - Mountain Trail.way_open (Insane Bolt photo chain) AND the body
#   L6 finale     - name Mrs Seadock to Kojak
#
# Only 33 clues exist and 32 are required, so essentially every one is on the
# critical path. Five of them come from RandomChance sneeze turnscripts (Miss
# Harlott 15%, Mrs Seadock 25%, Reverend Keen 25%, Colonel Custard 33%, Mrs
# Bright 50%) - hence the `wait` runs, which are load-bearing, not padding.
#
# Money is exact: $105 in (Rev Keen, the caged rat, Dusty Den, Mrs Hatch's
# handbag, Mrs Whippy's safe, Kojak's $100 reward) and $105 out (jar, pizza,
# beans, lolly, loaf, and the $100 toilet roll). The gourmet burger MUST be paid
# for with Small Daniels' toupee, not cash, or the toilet roll is unaffordable.
#
# Two RNG hazards are handled defensively rather than by lucky timing:
#   * every `port X` is issued TWICE. The teleporter has a one-shot 5% "fly in
#     the machine" gag that silently aborts the jump; the second call completes
#     it (or harmlessly reports "You are Already There").
#   * Mad Donna appears in a forest room with 30% probability on entry, and she
#     is needed twice - once to scrub the mud off the blue suede hooves (she only
#     does it if you carry neither the camera nor the dvd, so both are dropped in
#     the Countryside first) and once to be photographed (the dvd frightens her
#     enough that she no longer steals the camera). Both passes just circle
#     In the Trees / Forest Grove / Woodland Glade until she shows up.
#
# Other footguns encoded below: containers with hidechildren must be examined
# before their contents resolve (`x control console` before `x fluid link`,
# `x counter` before `ring bell`, `x car` before the milk churn, `x punctured
# tyre` before the nail, `x computer console` before the black button); the
# psychic paper is called "white card" until read three times; Coldfinger takes
# the glove via USE, not GIVE; the jar must end up completely EMPTY (gherkins to
# Mrs Bright, vinegar onto Jack's head) before it can act as the lens that
# reveals the bunker's hidden switch; and the axe has to survive until the very
# end, because chopping the beanstalk is the only way to make the reporters
# besieging the BARDIS finally leave.
#
# Result: state=Finished, 0 errors, 71 tasks completed, "CONGRATULATIONS! You
# have completed this adventure!"
# --- chunk 1: opening + village survey ---
answer:no
x me
in
x control console
x fluid link
take fluid link
take teleporter
out
s
e
s
speak to mrs hatch
x car
open boot
take car jack
read magazine
take contract
use car jack on car
s
e
speak to dusty den
speak to dusty den
x miss harlott
x bar
take beermat
give beermat to miss harlott
ask miss harlott about reverend keen
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
n
speak to norman ramsey
s
w
s
x flowerbed
x flowerbed
x flowerbed
x pennyfarthing
take bicycle pump
speak to number 66
s
s
search first aid cupboard
search filing cabinet
n
sw
speak to mr mcgregor
ne
n
w
n
speak to vidal monsoon
give bottle to vidal monsoon
take hairspray
take brush
cut plaster
s
s
x counter
ring bell
speak to kojak
speak to kojak
n
e
e
x rubbish
sweep rubbish
# --- chunk 2: money, jar, Jack, rat, youths ---
w
n
e
n
give blue crocuses to norman ramsey
take large piece of cheese
s
w
n
n
w
w
speak to sherlock bones
ask sherlock bones about matches
give silver cane to sherlock bones
s
w
speak to reverend keen
port c
port c
w
w
in
take jar
out
e
e
n
n
n
n
speak to monty python
give jar to monty python
s
w
w
w
use vinegar on jack
use brown paper on jack
speak to jack
port a
port a
nw
w
speak to rob geldof
give band aid dressing to rob geldof
give large piece of cheese to rob geldof
e
se
s
e
s
s
e
down
catch rat
up
port c
port c
w
w
in
take frozen pizza
out
e
e
n
e
n
heat frozen pizza
s
w
s
e
give heated pizza to group of youths
ask group of youths about barrel
w
n
e
speak to dusty den
port c
port c
w
w
in
take can of baked beans
out
e
e
n
n
n
w
w
w
give can of baked beans to jack
w
in
x jacks cat
e
take shovel
dig garden
in
out
e
e
e
x cow costume
take blue suede hooves
port b
port b
up
# --- chunk 3: Level 2 clouds ---
n
speak to rupert burdock
ask rupert burdock about dandelion wine
n
speak to the fake shaking stevens
n
take pot of salt
s
s
s
s
s
take huge tweezers
port a
port a
s
e
s
x punctured tyre
use huge tweezers on iron nail
use plaster patch on punctured tyre
pump tyre
use punctured tyre on car
n
n
take handbag
x car
x milk churn
take milk
# --- chunk 4: wine, blessed nail, Level 3 ---
s
w
w
s
w
give dandelion wine to reverend keen
take rubber stopper
give iron nail to reverend keen
e
n
e
e
n
give iron nail to monty python
port b
port b
in
use bottle on bowl
take ball of wool
port c
port c
s
e
e
e
e
x wall
x brick
take brick
in
speak to donald trumper
give pot of salt to donald trumper
give hairspray to donald trumper
x writing desk
open present
out
port c
port c
w
n
use pot of salt on vidal monsoon
port a
port a
give white tulips to mrs giant
give red roses to mrs giant
give huge tweezers to mrs giant
give hairdressing voucher to mrs giant
nw
w
in
search old wooden chest
w
n
# --- chunk 5: Manor House, money, house key, lolly ---
x shelves
x murder suspect list
x body
x overcoat
search pocket
s
n
speak to miss sharples
give ball of wool to miss sharples
use thesaurus on rubber stopper
s
port b
port b
up
n
n
give money to the fake shaking stevens
take house key
s
s
down
port c
port c
s
e
e
e
in
play piano
out
port c
port c
w
w
in
take ice lolly
out
e
s
give ice lolly to kojak
take notebook
take stick
# --- chunk 6: camera, Mad Donna, Insane Bolt ---
n
e
e
ne
in
speak to david camwrong
out
sw
w
n
n
n
w
n
nw
w
in
w
w
drop dvd case
drop camera
se
sw
se
n
sw
se
n
sw
se
n
sw
se
n
nw
take dvd case
take camera
se
use camera on mad donna
sw
use camera on mad donna
se
use camera on mad donna
n
use camera on mad donna
sw
use camera on mad donna
se
use camera on mad donna
n
use camera on mad donna
sw
use camera on mad donna
se
use camera on mad donna
n
use camera on mad donna
sw
use camera on mad donna
se
use camera on mad donna
n
use camera on mad donna
nw
port c
port c
e
ne
in
give camera to david camwrong
port a
port a
s
w
w
s
wear slowmo glasses
speak to insane bolt
port c
port c
e
ne
in
x latex mask
n
take photograph
out
port a
port a
s
w
w
s
give photograph to insane bolt
port c
port c
s
e
e
in
speak to hambo
port c
port c
n
n
n
n
n
speak to even job
give silk tie to even job
speak to even job
port c
port c
s
e
e
e
n
give cv to donald mcronald
# --- chunk 7: Level 5 mountains ---
port c
port c
w
w
w
s
s
x large boulder
s
use shovel on snowdrift
x pile of clothes
open briefcase
take route map
take secret papers
port c
port c
w
s
tell kojak about clothes
take bell
take brick
port d
port d
s
s
s
e
e
ring bell
use shovel on bee york
ne
e
x mrs bright
speak to mrs bright
give jar to mrs bright
wait
wait
wait
wait
wait
wait
wait
wait
w
sw
s
s
se
ask mrs whippy about jokes
e
take painting
use stethoscope on safe
w
nw
sw
w
s
# --- chunk 8: valley, pool, gnome ---
nw
speak to small daniels
s
speak to little chef
sw
speak to gnomer simpson
ne
n
se
se
in
x professor glum
speak to professor glum
in
nw
nw
s
sw
give babel fish to gnomer simpson
open rat trap
take magnet
take bowling guide
read bowling guide
# --- chunk 9: village round 2 ---
port c
port c
give passport to number 66
take passport
give dictionary to patrick mcgoogle
give secret papers to patrick mcgoogle
s
e
s
x mrs seadock
speak to mrs seadock
give passport to mrs seadock
give library book to mrs seadock
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
n
w
sw
use magnet on haystack
ne
n
w
w
give joke collection to michael mcintosh
take canister of laughing gas
port a
port a
nw
w
in
w
n
give metal knitting needles to miss sharples
take candlestick
# --- chunk 10: bowling, Sting, toupee, Hell ---
port a
port a
s
w
s
e
s
use bowling ball
n
speak to michael jockson
give rat trap to michael jockson
port d
port d
s
s
s
give dance guide to sting
take knotted rope
port g
port g
sw
w
s
nw
give knotted rope to small daniels
take toupee
port c
port c
s
e
e
e
n
give toupee to donald mcronald
port g
port g
sw
w
s
e
e
in
down
out
give contract to simon bowell
sw
x hell viss
give blue suede hooves to hell viss
give extra large gourmet burger to hell viss
ne
se
in
take herbs
out
nw
in
up
out
# --- chunk 11: caves, bunker, axe ---
port d
port d
s
s
s
s
sw
down
light candlestick
sw
s
se
w
give library book to jeremy paxo
take stuffing recipe
e
n
x wall of rock
press hidden switch
s
s
nw
use canister of laughing gas on sulk
n
ne
up
ne
n
n
tell sulk about boulder
# --- chunk 12: stuffing, vault, Custard ---
port c
port c
w
w
in
take loaf of bread
out
port g
port g
sw
w
s
nw
s
give stuffing recipe to little chef
give onion to little chef
give egg to little chef
give loaf of bread to little chef
give herbs to little chef
n
se
se
in
give explosive formula to professor glum
in
port c
port c
n
n
n
n
n
e
use glove on coldfinger
s
use flask of nitroglycerine on large vault door
in
take cuban cigars
take white card
read white card
read white card
read white card
port a
port a
s
w
w
s
s
sw
e
x colonel custard
ask colonel custard about scissors
use psychic paper on colonel custard
x desk
take pile of paperclips
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
w
ne
n
n
port c
port c
s
s
use paperclip on filing cabinet
x confidential file
n
# --- chunk 13: mercury, blanket, last clues ---
port a
port a
nw
w
in
w
w
w
n
n
fill fluid link
s
port a
port a
in
use fluid link on control console
out
s
w
s
e
use thesaurus on blanket
use thesaurus on blanket
port d
port d
s
s
s
e
e
ne
e
give blanket to mrs bright
port a
port a
s
w
s
w
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
read notebook
# --- chunk 14: the finale ---
port c
port c
w
s
menu:Mrs Seadock
port x
port x
speak to mrs seadock
speak to kojak
speak to mrs seadock
port x
port x
x computer console
press small black button
port c
port c
w
w
in
take toilet roll
out
port b
port b
chop beanstalk
port a
port a
tell reporters about doctor black
in
pull handle
take cardboard tube
use toilet roll on control console
pull handle
tasks
pull handle
