# Quest for the Serpent's Eye (Steven Hanulik / "Lazygamedesigner82", release 1.1.1) -- WINNING SCRIPT
#
# RESULT: state=Finished errors=0 -- genuine GOOD ending (Carter defeated, Eye returned to
# the altar, amulet handed back to Professor P, "THE END" + ship epilogue + author thank-you).
# 182 steps, deterministic at seed 1234 (no RNG in this game's puzzles).
#
# SOURCE: the author's own "Full Solution v1.1.1" (Quest for the Serpent's Eye -
# walkthrough.txt, converted from the ifarchive PDF). It is PROSE, not a linear script,
# so every step here is the prose intent rendered into the parser vocabulary verified
# against game.aslx. Deviations / derivation choices:
#   * Title screen asks "first time playing? (Y/N?)" -- "y" enters the 1983 tutorial,
#     which the walkthrough covers. Tutorial requires trying LOOK and USE before each
#     DONE; "read manual" enters a reading mode that needs "close manual" before the
#     disk can be taken; the loader is finished with "turn on computer" + "play game".
#   * Village: leaving the professor's parlour takes THREE easts to the trading post
#     (front-door room intervenes; the walkthrough says "East twice"). The yes to the
#     professor's bravery question is the bare command "y" (a room command, not an ask).
#     Chess hint is "tell professor about q-n8".
#   * Jungle: "fire slingshot at bananas" after "put marble in slingshot"; the vines are
#     "cut vines with knife"; flint is in the skeleton's vest ("look in vest pocket").
#     The bridge-break trigger needs the hint book carried AND the stone device seen, so
#     "look at stone device" at the outcropping is mandatory before re-crossing south.
#   * Ravine cave: the walkthrough's "put the paper in the rocks" means the HINT BOOK --
#     it is the only burnable paper and the fire script checks for it explicitly
#     ("put hint book in rocks", then "strike flint on rocks").
#   * Dark maze (v1.1.1 = 3 rooms): w to the echo room, "throw rock at wall", n to the
#     jagged room, "take glow" (= the compass), "look at compass" (unlocks the hidden
#     exits), e, "feel water", "swim in pool" (needs the compass held).
#   * Plateau: bare "lower rope" opens a disambiguation menu that does NOT include the
#     cliff -- the working form is "lower rope down cliff". The stone-device cryptogram
#     answer is the statement "what" (plaque: "GVaW iN WVe maDiJ Gord" = WHAT is the
#     magic word). Gate opens with "put key in keyhole".
#   * Temple: walkthrough map directions are correct as written. Exact verbs:
#     "use spear head on web", "attach spear head to broken pole", "use spear on floppy
#     disk", "put handle in lever" + "pull lever", "put disk in slot", "take jade snake",
#     "put jade snake in circle".
#   * Finale (turn-based SetTurnTimeout fight, no waits needed): "take eye" triggers the
#     Carter cutscene; "play flute" MUST be the next turn (cobra kills in 2 turns);
#     "throw peel at carter" the turn after (the walkthrough's "throw the banana" -- the
#     banana itself was eaten back at the grove, as the walkthrough itself instructs; the
#     throwable is the peel). Then "take eye" and the GOOD-ending choice "put eye on
#     altar" (the mutually-exclusive bad ending would be "steal eye" -- not taken).
#   * Return: "use plank on hole" mid-bridge, then "knock five times" at the log house
#     delivers the full good ending.
#   * Final "quit": the good-ending script parks the player in the game's Ending Room
#     without ever calling finish (only the death/despair endings call finish), so the
#     session would stay state=Running forever. The game's own quit command (^quit$ ->
#     finish) is sent once AFTER the complete ending text to close the session cleanly.
y
look at computer
look at game box
done
use computer
done
open game box
read manual
close manual
take floppy disk
put disk in drive
turn on computer
play game
n
look at pebbles
take marble
e
look at horse
ask stan about horse
w
w
i
read journal
put away journal
knock five times
w
ask professor about gate
y
tell professor about q-n8
look at chess board
take gold coin
e
e
e
give gold coin to stan
take slingshot
take knife
take flashlight
w
n
show amulet to guard
n
n
w
w
s
put marble in slingshot
fire slingshot at bananas
take banana
eat banana
s
cut vines with knife
look in vest pocket
take flint
n
n
e
e
e
look in log
take hint book
e
n
use vine on ropes
n
n
n
e
e
look at stone device
w
w
s
s
w
take sticks
e
e
n
put sticks in rocks
put hint book in rocks
strike flint on rocks
look
take page
read page
rest
n
w
take rock
w
throw rock at wall
n
take glow
look at compass
e
feel water
swim in pool
up
e
lower rope down cliff
n
take batteries
take letter
take plank
look in sleeping bag
take spear head
read letter
s
w
n
read plaque
s
e
down
e
what
take stone key
read writing
w
up
w
n
put key in keyhole
n
read plaque
n
put batteries in flashlight
turn on flashlight
w
w
n
n
use spear head on web
attach spear head to broken pole
s
s
e
e
e
n
e
use spear on floppy disk
s
put handle in lever
pull lever
s
take flute
n
n
w
s
put disk in slot
take jade snake
n
n
e
n
put jade snake in circle
n
take eye
play flute
throw peel at carter
take eye
put eye on altar
s
s
e
down
w
s
s
use plank on hole
s
s
w
w
s
s
s
w
knock five times
quit
