# The Mouse Who Woke Up For Christmas -- winning script (state=Finished, errors=0)
# Source walkthrough: David Welbourn (Key & Compass).
#
# Deviations from the raw walkthrough:
#  - Dropped every stray "Continue" line (walkthrough's "> Continue..." prose).
#    The game's "press any key to continue" pauses are auto-continued by the
#    oracle; an explicit "Continue" command just yields a harmless
#    "I don't understand your command" (no script error).
#  - Dropped "put bowl in moonbeam" (walkthrough itself notes the moonbeam is in
#    Jack's cage, not yours -- the command fails; "give bowl to Jack" is what
#    triggers Peter). Harmless either way; removed for cleanliness.
#
# NOTE: reaching the win also required an ORACLE fix (Program.cs DrainTimers):
#  the game gates progress on real-time SetTimeout timers -- the nightclub's
#  "eyes adjust to the dark" room reveal (game.aslx:3486) and the yard->town
#  MoveObject after three tasks (game.aslx:2910). The headless oracle has no
#  wall clock, so without draining these timers the nightclub stays pitch dark
#  and the town is unreachable. DrainTimers reproduces the interactive players'
#  1-second tick loop deterministically. No game-specific hacks; script is the
#  plain Welbourn walkthrough minus the two harmless lines above.
switch off clock
x me
i
x list
x johns
x pyjamas
x slippers
x Jessica
x note
x bed
x crib
x fireplace
x mantelpiece
x pipe
x hole
x mirror
x stocking
x fridge
x frame
x wardrobe
open it
x suit
x clothes
remove pyjamas
put pyjamas in wardrobe
take clothes
wear clothes
x mirror
x jeans
x t-shirt
x jumper
x boots
x toolbox
open toolbox
x tape
x fluid
x rag
out
x barbecue
x skull
x shed
ne
x spade
x hole
x coin
x Stewart
talk to Stewart
x stone
x wheelbarrow
nw
x pallet
x nail
take nail
x match
x heap
x ball
take ball
se
sw
s
x weasel
x machete
x tree
x midden
talk to weasel
in
u
x Peter
x shiny
x rings
x bar
x strap
talk to Peter
ask Peter about shiny
ask Peter about rings
ask Peter about strap
put ball in collection
take bar
d
x weasel
x flyer
take machete
in
x decorations
x Cyril
x floor
x ball
x bottles
take lights
x bar
x bar mirror
x Trevor
x Ken
x Sam
x ipad
talk to Cyril
ask Cyril about Harry
ask Cyril about Jack
ask Cyril about lights
x ipad
4
x light bottle
x floor
dance
talk to Ken
ask Ken about Jack
talk to Trevor
ask Trevor about Jack
ask Cyril about stoat
ask Trevor about stoat
ask Ken about stoat
ask Sam about stoat
out
n
take skull
in
take stocking
take rag
take tape
out
ne
take coin
polish coin
x coin
e
x scissors
x gate
x fur
take scissors
take fur
out
s
x tap
x boxes
s
x sponge
take sponge
x nettles
cut nettles
sw
x holly
take holly
ne
n
turn on faucet
x drain
drink water
x grating
use sponge on faucet
turn off faucet
nw
use sponge on stone
sharpen scissors
s
x bath
x Jon
x greenhouse
x page
read it
take it
talk to Jon
ask Jon about Jon
ask Jon about cat
ask Jon about weasel
ask Jon about Gemma
ask Jon about Peter
ask Jon about Petcetera
ask Jon about Christmas
ask Jon about Santa
ask Jon about Cyril
ask Jon about Harry
ask Jon about Stoat
ask Jon about Trevor
ask Jon about Ken
ask Jon about Sam
give skull to Jon
give fur to Jon
in
x cat
x paper
take paper
x twine
take twine
x table
x plant pot
take it
cut tape
use strip on plant pot
x bucket
out
e
turn on faucet
put bucket on drain
turn off faucet
x bucket
w
use bucket on bath
x nuts
sw
in
buy beer
out
u
give shiny coin to Peter
d
n
ne
give scrap of paper to snail
x Stewart
talk to Stewart
ask Stewart about Jack
nw
take match
use bar on nail
tie bar to skull
use crowbar on nail
se
sw
u
in
x Penelope
x sewing machine
x weaving machine
x lump
take lump
talk to Penelope
ask Penelope about Gemma
ask Penelope about Nancy
give stocking to Penelope
give holly to Penelope
give stocking to Penelope
x stocking
out
d
in
open fridge
put ale in fridge
close fridge
hang stocking
put lump in fireplace
take fluid
use fluid on lump
use match on lump
drop all
out
ne
e
out
x van
x snow
e
n
x Santa
x elf
x elves
talk to Santa
give note to Santa
x Jessica's present
open your present
x ninja
remove clothes
wear ninja
out
ne
e
x store
in
throw hook
listen
x window
open window
in
x banners
x balloons
x boxes
x chair
x desk
x straw
x map
x small cage
x Gemma
x rags
x large cage
x Jack
x bowl
x bottle
x moonbeam
x window
x bin
talk to Gemma
talk to Jack
take bowl
polish bowl
polish bowl
give bowl to Jack
talk to peter
out
open Gemma's cage
open Jack's cage
x bin
take clothes
take present
take ninja
wear ninja
out
put present in stocking
x list
wake Jessica
