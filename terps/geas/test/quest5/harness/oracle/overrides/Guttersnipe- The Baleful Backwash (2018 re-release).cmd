# Guttersnipe: The Baleful Backwash (Bitter Karella) -- 2018 RE-RELEASE -- WINNING SCRIPT
#
# WHICH BUILD: this row drives "Guttersnipe- The Baleful Backwash (2018 re-release).quest",
# the author's later build (game.aslx saved by Quest 5.8.6794.18055, 2018-08-12; cover
# backwash512.png; shipped as speakeasy5.quest). The long-standing corpus row
# "Guttersnipe- The Baleful Backwash" stays frozen on the earlier build (Quest 5.7.6404.15496,
# 2018-03-24, cover backwashs.png). Same gameid 9e5bdd86-3b6a-404a-ae18-7cd0a32af701, same
# 153 objects / 47 exits, both ASL v550 -- but the two are NOT interchangeable, and the
# earlier build's frozen script does NOT win on this one (it ends Running, 0 errors, having
# silently lost the poker key at "take bibles"). Hence a second row rather than a version bump.
#
# RESULT: state=Finished errors=0 -- the genuine ending (Chicago typewriter drawn on Tony
# Macaroni, Rags and Percy escape together) followed by the "Thanks for playing" credits and
# THE END. 294 steps, deterministic at seed 1234 (re-ran identical; no RNG in this game).
#
# SOURCE: the frozen golden/Guttersnipe- The Baleful Backwash.cmd -- i.e. David Welbourn's
# walkthrough as extracted for the earlier build (corpus row 33, mode=welbourn, no override)
# -- adapted to this build. FIVE deviations, all forced by author changes, all verified
# against this build's game.aslx:
#   * "x closeline" -> "x clothes line". The printing-room object is still named "closeline"
#     internally but now carries <alias>clothes line</alias>, and the alias is what the parser
#     matches -- "closeline" is now "I can't see that." (Cosmetic; the room prose was
#     re-spelled too.)
#   * "x bibles" / "take bibles" -> "x covers" / "take covers". The press output was renamed
#     "tijuana bibles" -> "Tijuana bible covers" (it now prints covers, not whole tracts), and
#     it has no <alias>, so "bibles" no longer resolves. THIS IS THE ONE THAT MATTERS: without
#     the take, Rags has no five-of-a-kind, never wins the poker hand, never gets the rusty key,
#     and the remaining ~200 steps run in the wrong rooms.
#   * "give intestines to Fats" -> "give guts to Fats". Taking from the diner plate still uses
#     "take intestines" (that scenery object kept its name), but the item it puts in inventory
#     was renamed "intestines2" -> "handful of guts".
#   * DROPPED one "s" after the winning "sit on stool", and DROPPED the later return trip
#     "n" + "sit on stool". This build collapsed the poker puzzle: the earlier build needed two
#     sits (first = Sally explains "five of a kind", second = win) and left you in the poker
#     room, so the walkthrough walked back for the second sit. Here a single sit while holding
#     the covers wins outright AND Mulligan throws you out into the backroom, so the extra
#     move and the whole return trip land in the wrong rooms. Also dropped the now-pointless
#     "x covers" at Percy's cell (the covers are spent on the table by then).
#   * "unlock trapdoor with key" -> "use key on trapdoor". The ceiling trapdoor gained a real
#     padlock: the earlier build had a scenery "trapdoor1" plus a custom "unlock trapdoor with
#     key" command pattern, whereas this build models it as a container_lockable "padlock"
#     (alt "trapdoor") whose key is reached through the key's <selfuseon> dictionary. Core's
#     generic "unlock X with Y" has no handler here and answers with the DefaultMultiObjectVerb
#     template ("That doesn't work."), leaving the exit locked and the whole apothecary/street
#     half of the game unreachable.
#
# Everything else in Welbourn's route survives verbatim, including the prose-typo fixes this
# build shipped (recuse->rescue, shaprel->shrapnel, "across the rope"->"across the room",
# love one->loved one), which show up only as transcript text.
x me
i
x still
x barrels
x rope
take it
x keg
push keg
x mousehole
look in it
x matchbook
take it
use rope on still
light fuse
n
x door
w
x Percy
x cage
ask Percy about Percy
ask Percy about ferret
ask Percy about Tony
e
n
x Murray
x bottle
x barrels
ask Murray about Murray
ask Murray about bottle
ask Murray about barrels
ask Murray about Percy
ask Murray about Tony
ask Murray about army
ask Murray about music
ask Murray about Luigi
ask Murray about me
ask Murray about Jocko
s
e
x trapdoor
u
e
x Tony
ask Tony about Percy
e
ask Tony about pops
s
x Hungarian
x ferret
x chalkboard
x poster
x file
x barker
ask Hungarian about ferret
ask Hungarian about Queen Molly
n
se
x Vinnie
x counter
x guns
ask Vinnie about guns
ask Vinnie about Papa
ask Vinnie about moll
ask Vinnie about Tony
nw
ne
x press
x clothes line
x bills
turn on press
x covers
take covers
sw
n
x Sally
x Mulligan
x Jimmy
x key
x stool
ask Sally about Sally
ask Sally about Jimmy
ask Sally about Mulligan
ask Sally about Tony
ask Sally about Papa
ask Sally about moll
ask Sally about Jocko
ask Sally about Vinnie
sit on stool
w
w
w
ask Percy about poker
ask Percy about moll
ask Percy about Luigi
ask Percy about Murray
ask Percy about Vinnie
e
e
e
w
use key on trapdoor
u
x shopkeeper
x barrels
ask Luigi about Luigi
ne
x pigeon
x nest
x trashcan
open trashcan
x boots
take boots
eat boots
x shoelaces
sw
d
n
x trunk
x Fats
open trunk
x record
take it
x sandbags
x props
ask Fats about Fats
ask Fats about guitar
ask Fats about catgut
ask Fats about Papa
ask Fats about Tony
n
x victrola
x goon
n
x dancers
w
x bartender
x terrier
x jingleball
x liquor
x bar
ask bartender about dog
pet dog
e
n
x bottle
x patron
x menu
x waiters
take bottle
ask patron about menu
ask patron about spaghetti
e
w
x plate
take it
x chefs
x waiters
e
ne
x carpet
x podium
x maitre
x menu
take menu
ne
sw
s
s
s
s
w
w
ask Percy about Fats
give menu to Percy
give record to Percy
ask Percy about pigeon
ask Percy about terrier
e
e
n
n
n
n
ask patron about intestines
x intestines
take intestines
s
s
s
give guts to Fats
n
x Fats
x jar
take victrola
s
s
w
n
put record on victrola
take rotgut
s
use rotgut on door
nw
x pipes
x valve
turn valve
se
e
n
n
n
n
e
x bidet
x toilet
x basin
x shave
take shave
w
s
s
s
s
e
ask Tony about moll
put shoelaces on plate
put sauce on shoelaces
give spaghetti to Tony
e
x Papa
x goons
x moll
x statue
x coin
give spaghetti to Papa
take coin
ask Papa about moll
w
w
w
w
give coin to Percy
e
e
n
n
put coin in jar
s
s
e
e
use shave on statue
x note
w
se
give note to Vinnie
x gun
nw
w
u
ne
shoot pigeon
x pigeon
take it
sw
d
n
n
n
w
give dead pigeon to dog
take jingleball
e
s
s
s
e
s
talk to Hungarian
challenge Jocko
x file
n
w
w
w
use file on cage
e
e
n
n
n
n
ne
shoot Tony
