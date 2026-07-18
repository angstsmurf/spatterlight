#!errorlimit=200
# Whitefield Academy of Witchcraft - WINNING script (state=Finished, errors=34)
#
# The script is David Welbourn's full walkthrough, split into one-command-per-line.
# It runs cleanly through the entire game -- rescuing Jenny, Renee, Traudel and
# Tally, the underwater sarcophagus, the library/Pluto detour, Mary Jane, Gwen,
# the floor door, the cottage and the beach -- to the genuine THE END (all
# students saved, epilogue at the Port Querubin police station) and `finish`.
#
# WHY THE #!errorlimit DIRECTIVE (QuestViva incompatibility, not walkthrough drift):
#  - The MANDATORY Grislewood snare (south from Frosted Forest, game.aslx line 3444)
#    teleports the player with a DOUBLE MoveObject: `MoveObject(player, Grislewood
#    Kitchen)` then immediately `MoveObject(player, Inescapable Cage)` (lines
#    3451-3452). "Inescapable Cage" is reached only by that teleport and never has
#    grid map coordinates established, so OnEnterRoom ->
#    Grid_CalculateMapCoordinates(cage) -> DictionaryItem(coordinates,"x") throws
#    ~19 times in that single transition (8x 'x' + 8x 'y' + 'z' + 2 more).
#  - QuestViva's 20-script-error circuit breaker (which legacy Quest does not have;
#    it printed script errors and carried on) then kills the session -> Wedged,
#    despite the game being fully playable and winnable in real Quest.
#  - The error storm is FINITE: 34 errors across the whole 440-step game (the cage
#    burst plus a few later map recalculations). #!errorlimit=200 lifts the breaker
#    past it; every error is still printed in the transcript, faithfully matching
#    legacy Quest's carry-on behaviour. Same pattern as The Acreage.
x me
x wand
x spellbook
x id
x parcel
open parcel
wear uniform
x civvies
x sandwich
eat it
x knapsack
e
n
x bushes
take blackberries
eat blackberries
s
e
x fountain
x nymph
x gnome
x key
take key
e
s
x craters
s
x greenhouse
w
x vegetables
x shed
in
x manual
read it
out
n
n
cryoglaze fountain
take key
use key on door
e
x mirror
ask mirror about mirror
e
x punchbowl
x banner
x Gwen
x puddles
aurale punchbowl
aurale puddles
aurale Gwen
x gingerbread
aurale gingerbread
s
e
n
e
x vines
n
e
x bookmarks
take bookmarks
x vandalized
open it
take crackerjack
x it
eat it
x dirty desk
open it
x stinky desk
open it
x blackboard
read it
w
n
x terranium
x textbook
aurale it
take it
use bookmarks on textbook
take textbook
read it
drop textbook
x jar
read jar
x faeries
take jar
s
s
w
s
w
n
read yeggle
x id
envive id
ask id about yeggle
x id
x basket
eat basket
n
x painting
aurale painting
x cabinet
open cabinet
read brandt
read finch
read grislewood
read leblanc
read marx
read mcbride
read minsky
read potts
read sotherby
read yoshida
x desk
yeggle drawer
read hannigan file
take it
x apple
take it
x monkey
aurale monkey
take monkey
s
w
x socks
aurale socks
x grenade
take grenade
x moose
envive moose
x moose
x trashy
read trashy
aurale trashy
e
s
w
w
envive nymph
envive gnome
use sock on fountain
s
s
yeggle greenhouse
w
x bedding
x uniform
take it
x horror
aurale horror
open jar
drop jar
x notepaper
ask Jenny about notepaper
give apple to Jenny
x pie
eat pie
x textbook
read textbook
x set
ask Jenny about Gwen
ask Jenny about Mary Jane
ask Jenny about Renee
ask Jenny about Traudel
ask Jenny about Tally
ask Jenny about Hannigan
ask Jenny about Potts
ask Jenny about Jenny
ask Jenny about me
ask Jenny about silver
ask Jenny about gold
ask Jenny about bread
ask Jenny about plant
e
n
n
e
e
e
s
x barricade
use sock on barricade
kiss notepaper
drop notepaper
e
s
w
x fantasy
read it
take it
x lurid
read it
e
sw
x red flag
take it
ne
s
x dispenser
open cabinet
take tweezers
take sponge
x it
n
se
x letter
read letter
nw
e
w
n
e
n
x window
e
s
x washing machine
open it
x raven
envive raven
put flag in machine
close machine
pull lever
open machine
take flag
x flag
se
s
x Princess
x cabinet
open it
x jodhpurs
x extractor
take it
n
nw
n
w
s
w
w
n
w
w
w
use extractor on gnome
drop extractor
envive gnome
take gnome
e
e
e
s
e
e
n
use sock on window
w
x Tally
x silverback
x gnomes
x mound
drop garden gnome
take Tally
x shovel
take it
e
s
w
w
n
w
u
s
x china
n
n
x automat
talk to automat
give fantasy to automat
ask automat about poseidon's
read yarn
give yarn to automat
ask automat about mother
read embarrassing
give it to automat
ask automat about pluto
read sci-fi
x trees
n
w
w
wave flag
w
x Jack
talk to Jack
kiss Jack
x Jack
in
x dispenser
push button
x specimen
take it
out
x pill
use tweezers on pill
x pill
eat it
drop tweezers
e
e
s
s
give sci-fi to automat
s
d
w
w
w
w
mermaphro me
w
d
x salmon
eat it
d
in
x darkness
aurale darkness
x sarcophagus
open it
cryoglaze sarcophagus
out
u
u
e
mermaphro me
e
e
e
e
u
s
x dust
x sarcophagus
open it
x coffin
yeggle it
x mummy
unwrap mummy
x gold
take it
n
d
use sponge on puddles
s
s
s
x Mary Jane
x shelves
x trophy
x juice
x canister
read it
envive trophy
x trophy
x punch
cryoglaze punch
take canister
eat canister
out
x baba
push baba
x knife
take it
s
x pumpkins
take pumpkin
use knife on pumpkin
x jack
envive jack
x purse
open it
x prescription
x lucky strikes
take lucky
x pills
take pills
n
put pills in jack
give jack to baba
take rye
n
n
n
w
w
s
s
w
give sponge to Jenny
give shovel to Jenny
give foil to Jenny
give rye to Jenny
give specimen to Jenny
x oil
e
n
n
e
e
use oil on Gwen
give monkey to Gwen
x glasses
n
n
d
se
e
x preserves
aurale preserves
u
x cereal
read it
aurale it
eat it
s
x Henrietta
x radio
turn on radio
give Hannigan file to Henrietta
e
x Packard
x headmistress
cryoglaze headmistress
envive shield
mermaphro me
yeggle Packard
