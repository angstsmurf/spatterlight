# WAKE — "Evolution through Extinction" (Timothy Sibiski / "Asyranok",
#   Demo version 2.2, v1.05, 2013, ASL 540) — MAXIMAL walkthrough.
#
# NOT A WIN: this demo has no `finish`, no "THE END", and its final puzzle is
# UNREACHABLE. Oracle reports state=Running errors=0 steps=113. The script below
# reaches the furthest state the game can reach, plus all the optional content.
#
# THE TERMINAL BUG (verified, faithful — not an oracle artifact):
#   `Activate MARS` (Control Room) calls ChangePOV(RCSR 02), so game.pov becomes
#   the robot. Driving the robot east into the Reactor Core Room and choosing
#   `attempt reset` runs ReactorCoreActual.attemptreset, which does
#     MoveObject (player, ReactorPuzzleRoom)
#   — it moves the *player* object, NOT game.pov. So the ten fuel rods end up in
#   a room the point-of-view is not in, and ReactorPuzzleRoom has no exits.
#   Asserted at the end of the run:
#     game.pov = RCSR 02          PASS
#     game.pov.parent = ReactorCoreRoom   PASS
#     player.parent = ReactorPuzzleRoom   PASS
#   `shift rod 1a` is therefore out of scope and does nothing. Real Quest 5
#   resolves scope from game.pov exactly as QuestViva does, so the demo is
#   genuinely unfinishable. The author's own placeholder confirms it — each rod's
#   win branch reads: "That did it! ... (Placeholder message. Need bunker door to
#   be unlocked)". Even if solved it would only unlock MilitaryBunker, a room
#   whose description is msg(""). SubwayStation/TracksSequence/BunkerHUB are
#   orphaned objects, never reachable.
#
# THE ROD PUZZLE (solved on paper, for the record — it is a lights-out variant):
#   Initial positions (an empty <position /> element is boolean TRUE in ASLX;
#   verified by assert): 1A=T 2A=F 3A=T 4A=F 5A=T / 1B=F 2B=F 3B=T 4B=T 5B=T.
#   Toggle sets: shift nA -> {nA, nB}; shift nB -> {nB, (n+1)B}; 5B has no shift
#   script at all. Goal = all ten TRUE. Solving over GF(2) gives the unique
#   5-move solution:  shift Rod 2A, Rod 4A, Rod 1B, Rod 2B, Rod 3B.
#   (Had every rod started FALSE, as the author probably intended, the answer
#   would instead be the trivial "shift 1A..5A".)
#
# OTHER AUTHOR BUGS AVOIDED HERE:
#   - `search for tank emblem tank` raises "Too few parameters passed to ShowMenu
#     - only 3 passed, but 4 expected" (the game calls ShowMenu with 2 args + a
#     callback; Core needs 3 + callback). Optional flavour, so it is left out to
#     keep errors=0. Add it back if you want the error reproduced.
#   - RoboticsRoom.audiocouplings is `visible=false` and nothing ever calls
#     MakeObjectVisible on it, so the Core Prep Room survivor recording can never
#     be repaired ("Audio couplings inoperable" forever).
#   - `unlock crate` / `open crate` resolve to the Alien Crate Key, whose alt list
#     contains "crate" and "supply crate". Say `unlock alien supply crate`.
#   - Every `take bone fragment` opens a disambiguation menu because the fragments
#     already in inventory stay in scope; option 1 is always the un-taken one.
#
# CODES AND ANSWERS:
#   Constellations (evolution order, aquatic -> terrestrial):
#     Gamma Wazen 1, Bios Magnis 2, Xiu Helios 3, Muru Nakat 4,
#     Muru Junkii 5, Hyperio Scionus 6, Sigma Bolis 7, Goyo Vignat 8
#   Pawn shop safe: 147888   (the scale reads 147 for you AND for the phone;
#                             "Location, location, location" -> 888)
#   Iron door keypad: 2700245830   (Ripley's dog tags, 270-024-5830)
#   Magic 8 ball riddle: octopus
#   Reactor terminal password: LIFE86Z0J   (never hinted anywhere in the demo)
#
# --- Part One: Hyper-stasis Room ---
2
Ash
talk to duffy
1
talk to duffy
1
# --- Part Two: Medical Station ---
ask duffy about key
unlock alien supply crate
touch strange device
# --- the Cold Void: order the constellations by evolution ---
look at flashing lights
ask for advice Help Beacon
move constellation muru nakat
4
move constellation goyo vignat
8
move constellation bios magnis
2
move constellation muru junkii
5
move constellation sigma bolis
7
move constellation gamma wazen
1
move constellation xiu helios
3
move constellation hyperio scionus
6
activate portal
# --- Outside the Medical Station: the time capsule ---
north
look at busted time capsule
take cell phone
# --- Fork in the road: only the Streets path exists in the demo ---
north
north
# --- Tank on the curb (east): bones 1 & 2, pole, dog tags ---
east
open tank
take bone fragment
1
take bone fragment
1
take metal pole
take dog tags
take grenade pin
west
# --- Refugee centre (west): bones 3 & 4 ---
west
look at lover's message on the wall
use cell phone on scrolling marquee
take bone fragment
1
look at aluminum slab
remove debris
east
# --- Streets: bone 5, then bridge the live wires ---
take bone fragment
1
complete skeleton massive skeleton
north
# --- Pawn shop (west): the safe, the mirror, the 8 ball ---
west
look at painting behind counter
flip painting behind counter
stand on scale scale
use cell phone on scale
unlock safe
147888
take mirror
take magic 8 ball
ask about riddle
answer riddle octopus
look at unusual ceramic pigeon
east
# --- Courtyard: power the iron door ---
clean panels solar panels
use mirror on solar panels
look at generator
use metal pole on bundle of severed wires
look at iron bulkhead door
look at engraved note
enter code keypad
2700245830
east
# --- Part Three: Military complex ---
1
look at steel bunker door
# --- Auditorium (east): the lead canister ---
east
look at stage
take lead canister
open cap lead canister
west
# --- Hallway (north) and barracks (east): the locker ---
north
look at control panel
open binder
look at Robotics Control Manual
east
kick closed locker
take radiation suit
take Geiger counter
west
# --- Underground, via the auditorium hole (needs the suit) ---
south
east
wear radiation suit
climb down into large hole
look at framed map
north
look at window
south
climb up through ceiling hole
take off radiation suit
west
north
# --- Geiger counter + open canister unlocks the Control Room ---
use Geiger counter on lead canister
northwest
activate computer terminal
LIFE86Z0J
Activate MARS
# --- You are now the MARS robot, deep in the reactor level ---
north
east
attempt reset Reactor Core
