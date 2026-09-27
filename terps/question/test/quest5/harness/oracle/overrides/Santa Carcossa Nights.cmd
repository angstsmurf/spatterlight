# Santa Carcossa Nights -- Bitter Karella, 2018 (Quest 5.8.6794, ASL 580) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 76 steps. Reaches the "humanity intact"
# ending: after refusing Prof. Abyss you step back out to the plaza where Babs
# the shopkeeper offers you a cigarette and you watch the cult file down to the
# sea -- "you're ending your days with your humanity intact" -> finish.
#
# SOURCE: no published walkthrough exists; derived entirely from game.aslx. A
# cosmic-vaporwave horror adventure (you, a one-eyed Gobbler survivor, return to
# the dying resort town of Santa Carcossa to investigate the cult "The Way of the
# Dolphin"). 36 rooms, ~278 objects, nine locked exits that gate the map.
#
# TWO ENDINGS (identical prerequisites, both call finish):
#   * REFUSE  -- answer "2" (No) to "Will you join us?" -> pushed back to the
#     lobby, Babs appears at the plaza sea wall, walk north -> cigarette ending.
#     This is the script below (the human / "good" ending).
#   * JOIN    -- answer "1" (Yes) instead of "2" -> the Gobbler takes your
#     remaining eye, "your transformation is beginning" -> finish immediately in
#     the office (the trailing "north" is then unused). The horror ending.
#   The Abyss prompt renders as a NUMBERED text menu (1: Yes / 2: No), so it is
#   answered with the integer, not the word "no".
#
# THE PUZZLE WEB (all nine locked exits, and what opens each):
#  1. motel room S -> parking lot : take the Dolphin "paper" from the open
#     suitcase (NOT the tourist "flyer"/brochure decoy beside it).
#  2. path up -> cliff top        : `use shark jaw with cliff` (auto-climbs).
#  3. cliff top in -> lighthouse  : `use combination lock`, enter 7657.
#  4. lighthouse down (after key) : the corpse "Low-dive Jenny" blocks the stair;
#     `make molotov cocktail` (flask + matchbook) burns her, reopening it.
#  5. motel office E -> inner office : auto-unlocks when you take the office key
#     from the ledge nest (which also wakes the corpse -- fixes 4).
#  6. boardwalk5 S -> hotel       : answer the acolyte's riddle. Reading the
#     notebook (inner office desk) teaches the answer "the fish cares not for the
#     shoal"; then `speak to acolyte` twice (intro, then he steps aside).
#  7. hotel S -> office           : `give powder to girl` -- the marine snow
#     (bluish-green powder, inner office minifridge) buys off the wall-eyed guard.
#  (Two more locked exits gate the tide route into "under the pier" -- solved by
#   `spin sphere` in the lantern room -- but that path is only an alternate way to
#   identify the marine snow and is unused here; see below.)
#
# KEY-ITEM CHAIN / notable gotchas:
#  * MARINE SNOW ID: taking the powder needs you to first know what it is. The
#    aquarium pamphlet gives the hotline number; `use telephone` in the motel room
#    and dial 1-800-654-MAMS -> the recording confirms the "bluish-green hue" and
#    sets the flag. (Alternative: `touch bodies` twice under the pier after the
#    tide recedes -- longer, so we phone it in.)
#  * SUNGLASSES: on the beach-3 corpse. `take sunglasses` TWICE (first attempt is
#    a moral-qualms refusal). Taking them lets you read the arcade Polyglot
#    cabinet's high score "LAMP 7657" = the lighthouse combination. (The dry-erase
#    scoreboard's "LAMP 6576" is a reversed-digit red herring.)
#  * SHARK JAW (aquarium): hangs on the wall until knocked loose. Up to the AV
#    room, `turn volume knob` (max), back down, `press button` -- the loud film
#    shakes it to the floor -- then `take shark jaw`. Needed to climb the cliff.
#  * FLASK: `search lighthouse keeper` (the cadaver's parser noun is its alias)
#    spawns the alcohol flask from his slicker; `take flask`. `take matchbook` is
#    already in the lantern room. Together they make the molotov for the corpse.
#  * OFFICE KEY: `search nest` on the ledge (NW of the lantern room).
#
# The one harmless non-fatal note is the opening Valerie dialogue ("No. Not yet.");
# every command below resolves cleanly (0 "can't see that" / "can't go there").

# --- Motel room: Dolphin paper (unlocks south) + phone to ID the marine snow ---
open suitcase
take paper
use telephone
1-800-654-MAMS
south
# --- Parking lot -> boardwalk hub ---
west
# --- Beach: sunglasses from the corpse (two tries) ---
west
down
north
north
take sunglasses
take sunglasses
wear sunglasses
south
south
up
east
# --- Arcade: read the Polyglot high score -> combination 7657 ---
south
south
south
east
play polyglot
west
# --- Aquarium/theater: shake the shark jaw off the wall ---
south
east
east
up
turn volume knob
down
press button
west
take shark jaw
# --- Path -> climb cliff -> open lighthouse (7657) ---
west
north
north
north
north
north
use shark jaw with cliff
use combination lock
7657
in
up
# --- Lantern room: flask + matchbook, then the ledge nest for the office key ---
search lighthouse keeper
take flask
take matchbook
northwest
search nest
southeast
# --- Burn the risen corpse blocking the stair, escape back down ---
make molotov cocktail
down
out
down
south
# --- Inner office: notebook (learn the riddle) + marine snow powder ---
east
east
east
open desk
take notebook
read notebook
open minifridge
take bottle of bluish green powder
# --- Endgame: pass the riddle, bribe the guard, refuse Abyss, meet Babs ---
west
west
west
south
south
south
south
speak to acolyte
speak to acolyte
south
give powder to girl
south
2
north
