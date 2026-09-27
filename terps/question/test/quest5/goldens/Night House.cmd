# Night House (Bitter Karella, IFComp 2016) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0 (150 steps, no failed commands) -- genuine "light world"
# ending: after the ghost boy is sent home, "open front door" plays the full finale
# ("You step out onto the front stoop ... Good luck out there.", end.png) and calls finish.
#
# SOURCE: "Night House - Walkthrough.txt" -- the author's own hints-style Q&A walkthrough
# (not a linear script). Route linearised from the hints; exact vocabulary verified against
# the unzipped game.aslx. Win condition (from source): giving the Atari cartridge to the
# ghost sets pit.ghostgone; both endings (front door / pit) check only that flag.
#
# Deviations / derivation choices (all verified against game.aslx):
#   * "look at dark figure" is REQUIRED before the dressmaker's dummy can be used at all:
#     at stage 0 its alias is "dark figure" (so "dummy" doesn't resolve) and the first
#     examine is what advances its stage counter to make it pushable.
#   * "look at sewing machine" before "take smock": the smock sits under hidechildren,
#     invisible to the parser until an examine clears it (walkthrough: "investigate the
#     sewing machine"). The walkthrough's "shift" garment is the smock (alt name).
#   * Walkthrough's "OPEN BOX WITH SCISSORS" -> "use scissors on strapping tape" (there is
#     no "open X with Y" pattern; the scissors' selfuseon hook keys on the tape/box).
#   * Walkthrough's "EMPTY the box" -> "take scrap books" (the take script is what dumps
#     them on the floor and makes the box light enough to push).
#   * Walkthrough's "DROP BABY MONITOR IN PIT" -> "put baby monitor in pit" (the drop-
#     pattern routes to the core put command, which rejects the pit as a non-container;
#     "put ... in ..." goes through the game's do_connect hub).
#   * Walkthrough's "PLANT DECOY" -> "push dummy": no plant script exists on the dummy;
#     pushing it with all 4 items attached is what triggers the Catskin/maw finale.
#   * The decoy is assembled in the MUSHROOM FOREST (where the dummy lands after being
#     pushed out the attic window and off the roof), not the backyard as the hints imply.
#   * Mittens must be WORN (not just carried) before taking both the frozen ground beef
#     and the dead raccoon; beef is defrosted via "put ground beef in oven" (the follow-up
#     wait prompt is auto-continued by the harness).
#   * The spider's cartridge is "abandoned object" until examined ("look at object"), which
#     also drops it into the webbing; taking it requires the sticky detached tendril.
#   * Skipped optional research beats (computer/floppy disk, encyclopedia, Betamax tape,
#     attic floorboard map, most baby-monitor Q&A): the ghost relocates to the garage
#     automatically after the scripted first meeting in the downstairs hallway and again
#     when the cartridge is taken, so the garage map is not needed for the win.
#   * Kept the baby monitor recon (drop into pit) and the final "ask baby monitor about
#     prickles" -- the walkthrough's "vitally important piece of information" -- which is
#     the story beat that presents the two endings. Chose the front-door "light world"
#     ending; the mutually-exclusive alternative ("get in pit") is the darkworld ending.
#
# --- upstairs: pee first (unlocks the rest of the house)
s
s
s
use toilet
n
w
open dresser
open magic wand
take batteries
take baby monitor
e
n
w
look under bed
take flashlight
put batteries in flashlight
open nightstand
take scissors
e
s
d
s
se
open junk drawer
take screwdriver
w
s
unscrew vent
d
search nest
up
n
e
e
turn on flashlight
e
take traffic cone
unlock car
in
turn on overhead light
read roadmaps
out
w
w
nw
d
open trunk
take grabber claw
use scissors on strapping tape
take scrap books
take bottleopener
push box
put baby monitor in pit
up
n
up
n
open trapdoor
up
look at dark figure
look at sewing machine
take smock
wear smock
open window
push dummy
push dummy
use bottleopener on drywall
up
push dummy
in
d
s
d
s
s
s
s
e
s
open cabinet
take leverlock key
take jack o'lantern
n
w
n
n
n
in
take mittens
wear mittens
take winter coat
out
se
open freezer
take ground beef
put ground beef in oven
w
s
d
take raccoon
up
s
s
w
s
put jack o'lantern on dummy
put traffic cone on dummy
put winter coat on dummy
put ground beef on dummy
push dummy
take tendril
n
e
n
n
n
n
n
up
n
up
s
e
s
give raccoon to spider
look at object
take cartridge
n
w
n
d
s
d
s
se
e
e
give cartridge to ghost
w
w
nw
n
up
n
e
ask baby monitor about prickles
w
s
d
open front door
