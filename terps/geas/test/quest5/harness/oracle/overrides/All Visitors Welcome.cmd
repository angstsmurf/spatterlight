# All Visitors Welcome (Bitter Karella, 2017, ASL 550) — shipped as Park.quest,
# renamed here to its title. No published walkthrough exists anywhere; this
# 89-step script was derived from the game source. It reaches the game's only
# winning `finish`: returning artifact x-6768 (the mummified Sloane child) to
# Sloane Rock, which is what Charlene Danning's phone call and the Pombo legend
# have been telling you to do all along. Every other `finish` in the game is a
# death (caught in the pitch-dark woods, or taken by Jask'akno).
#
# Route notes, all verified against the source:
#  - `check messages` is a VERB (property checkmessages), so it needs its object:
#    `check messages telephone`. Bare `check messages` is "I don't understand".
#  - `x book shelf` and `x cork board` are load-bearing, not flavour: their
#    firsttime scripts MoveObjectHere() the two research binders and the memo/
#    note, so nothing is researchable or readable until you have looked at them.
#  - `x rattle` (firsttime) is what EnableTimer(phone)s. Quest timers are
#    REAL-TIME, so a headless replay never fires them — hence the single `tick:5`
#    once we are back in the office, which fires the phone timer's first stage
#    (SetObjectFlagOn telephone.danning) so `answer telephone` delivers Danning's
#    "Take him back!" warning instead of a dial tone. Without the tick the call is
#    simply unreachable headless. It is the only tick in the script: after the
#    hallway break-in below, ticking would eventually fire jaskakno and kill you.
#  - The glove-box key is `a-2 key` (the object is named `a2 key` with alias
#    "A-2 key"); `take a2 key` is "I can't see that."
#  - Toolshed: `open door` in footpath2 is the room's own command and consumes
#    the A-2 key. Coming back out it is tool shed -> north -> footpath2 -> north
#    -> footpath -> west -> parking lot (two norths, not one).
#  - `light flare` only works outdoors, i.e. standing in the parking lot; it
#    SetLight()s all four trail rooms and unlocks the `dark` exit north. Entering
#    an unlit trail room instead starts a two-stage SetTimeout that kills you.
#  - The clearing's beforefirstenter is what sets player.cursed. That flag is the
#    gate on BOTH the hallway crowbar break-in ("You'd have to be in dire straits
#    to even consider it") and on looting the till — so the trip to the rock has
#    to happen before, not after, the ranger's office. Hence two trips out.
#  - The flare is not consumed on the way back: its burn-out is likewise a
#    real-time timer, and even in a live run 15s/stage leaves the whole interior
#    detour comfortably inside its life, so one flare covers both trips.
#  - Crowbar chain: `break glass case` -> `take rattle` (the rattle is the
#    anti-Jask'akno charm the plaque describes), then `open door` in the hallway
#    busts the ranger's office open, where the desk drawer holds the scrap of
#    paper reading "4488" — the artifact-storage keypad code. `use keypad` asks
#    for it via `get input`, answered by the bare `4488` line.
#  - Artifact storage: `take sheet` is what MakeObjectVisible()s the mummy.
#
# Deterministic: no RNG and no score anywhere on this path, and the one authored
# timer that fires does so only because the script asks for it.
# state=Finished, errors=0, 89 steps.
down
check messages telephone
x sticky note
read sticky note
x book shelf
ask folder about roy jones
ask folder about artifact x-6768
ask folder about charlene danning
ask folder about larry sabler
ask binder about lazarus sloane
ask binder about jaskakno
west
take flares
east
east
x cork board
read note
read memo
east
east
x rattle
read plaque
west
west
west
switch on computer
x computer
tick:5
answer telephone
east
east
east
north
north
x trailhead marker
in
open glove box
take a-2 key
out
east
south
open door
south
take crowbar
north
north
west
light flare
north
north
north
north
x sloane rock
read graffiti
south
south
south
south
south
south
break glass case
take rattle
west
open cash register
take cash
west
open door
north
open desk
read scrap paper
x radio
south
west
use keypad
4488
south
take sheet
take mummy
north
east
east
east
north
north
north
north
north
north
put mummy on rock
