# Train ("Train2019.quest", anonymous, first published 2015, ASL 550) - winning
# walkthrough, the game's only survivable ending.
# Derived from source; no published walkthrough exists.
#
# You come to in compartment 1 of a moving train with a corpse at your feet and
# no memory. The whole of act 1 runs on a hard turn clock (player.turns), so the
# long `z` runs below are load-bearing, not padding:
#
#   turn   7/8  a note is slid under the door; compartment 14 unlocks
#   turn    50  the maid cleans compartment 1; "Heenrich" turns up in the lobby
#   turn   150  the three targets sit down at table 7
#   turn   155  `recordingmade` checks Contains(table 7, tape recorder) AND its
#               "recording" flag - miss either and player.missionfailed is set,
#               which is a guaranteed death at turn 174
#   turn   174  `missionfailedrecordingturnscript` drags you into compartment 14
#   turn   250  the train stops at Mittstadt; act 2
#
# FOOTGUNS
#  * The bloody jacket is a death sentence: while you carry it outside
#    compartment 1 the `bloodyjacket` turnscript arrests you at 22%/40% a turn.
#    Ditto the body - the maid enters compartment 1 at turn 50 no matter where
#    you are, and `Maidfindsbody` then sets huntontrain (35%/turn arrest). Both
#    go out the window in the first six moves.
#  * `throw out body` needs the body IN HAND (the room command tests Got()), so
#    `take body` first; `open window` first of all.
#  * Every ShowMenu here is Core's *text* menu - answer with a bare number. A
#    number sent when no menu is pending is "I don't understand"; a menu left
#    unanswered silently eats every later command.
#  * The red box's contents are hidden (`hidechildren`): `x red box` after
#    opening it or the recorder is not in scope at all. Same for `x counter`
#    (the free 10 Thaler) and `x trash can`.
#  * The luggage-car net can only be climbed FROM the chair, and its climb
#    script always dumps you on the far side - the way back is
#    kick hatch -> enter hatch -> `pull grating` (which yields the sharp piece
#    of grating) -> `cut wires`. Do not go out onto the ledge/roof: the wind,
#    the corner and letting go of the roof are all lethal RandomChance rolls.
#  * `x boxes` while drunk hits an empty `if ()` in the author's script. Find
#    the box sober.
#  * The 50-Thaler bribe in the front of the luggage car is unavoidable - the
#    scene fires on entering that room carrying the recorder, and it is the only
#    way back into the passenger cars. Money: 85 to start + the 10 on the
#    restaurant counter, 50 to the bribe, 8 drinks at 5 = the wallet ends on 5.
#  * The poisoned drink in compartment 14 CANNOT be refused: while the big man
#    is there the exits are "Forget it pal" and `themanandthedrink` shoots you
#    within about two turns. Drink it on the very next turn, then
#    `move aging man` (compartment key), `search aging man` (the motel clipping)
#    and `unlock door`.
#  * The cure is the liquor, exactly as the villain says in the ending ("Ironic,
#    that liquor finally saves a life"): traindrink's script only makes you vomit
#    - which clears poisoned0-3 - when player.drinksinblood is already 5, so the
#    eight drinks must be bought BEFORE any are drunk (the bartender refuses a
#    drunk) and then downed in a row. Each `drink drink` raises a
#    disambiguation menu over the identical clones; answer `1`.
#  * Do NOT visit compartment 14 voluntarily after turn 155. The turn-174
#    summons is not guarded against having already been, so you get poisoned a
#    second time. Waiting for it costs nothing and only happens once.
#  * Act 2: both roadblock exits (`east` from the roadblock, `south` from the
#    north-south street) are instant arrests, wig or no wig - the wig in the
#    station trash can is a trap, taken here only to prove it.
#  * The dream is unavoidable and has two exits; `open window` in the dream
#    compartment is the deterministic one. It sets player.policehotel, which
#    only bars the hotel room's south door - the alternative (the restroom
#    nightmare, `drink drink` from the fat man) sets player.hotelpolice and
#    starts a 10-turn police countdown instead.
#  * The bookstore fight is pure RandomChance and can kill you: once the coated
#    man lands a punch (player.hitonce) every further turn is a 25-45% death.
#    hit/hit/kick is the sequence that wins under the oracle's seed-1234 stream;
#    tipping the small shelf over first buys the extra turn that makes it fit.
#  * The endgame is on a timer at both ends: in the office the bald man returns
#    on turn 5 (which frees your leg) and shoots you on turn 7, so `kick man`
#    immediately; in office1 you have exactly two turns - `take paperweight`,
#    `throw paperweight`.
x body
search body
open window
take body
throw out body
throw out jacket
take note
read note
s
e
e
e
enter compartment 14
3
e
stand on chair
climb wires
x boxes
open red box
x red box
take tape recorder
x tape recorder
press button
kick hatch
enter hatch
pull grating
w
cut wires
w
1
w
w
w
w
w
w
x counter
take money
put tape recorder on table 7
buy drink
buy drink
buy drink
buy drink
buy drink
buy drink
buy drink
buy drink
e
x picture
read newspaper
x chair
talk to man
3
e
e
e
e
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
drink tall drink
move aging man
search aging man
x newspaper cut-out
unlock door
out
drink drink
1
drink drink
1
drink drink
1
drink drink
1
drink drink
1
drink drink
1
drink drink
1
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
z
n
x trash can
take wig
n
w
x hotel
enter hotel
talk to manager
1
up
w
enter room 204
open window
sleep bed
open window
z
z
z
z
z
z
z
z
z
z
z
z
climb through window
e
climb drainpipe
down
e
n
e
tip over small shelf
z
z
hit man
hit man
kick man
x dead man
w
n
z
z
z
z
kick man
take paperweight
throw paperweight
