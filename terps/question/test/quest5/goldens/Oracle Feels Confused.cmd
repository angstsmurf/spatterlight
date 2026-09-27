# Oracle Feels Confused (ratakim, 2023, ASL 580) - winning walkthrough
# Derived from source; no published walkthrough exists.
# The Oracle's brain-eating disease has left him unable to work his medicine
# dispenser. Feed him three times for the three hints, then assemble the chain:
# ruby -> code -> vault -> key + spark plug -> jeep -> kayak -> beast -> his
# social security card -> the dispenser -> the pill. Reaches Core `finish`.
#
# FOOTGUN: this game uses `hidechildren` on every already-open container, and
# only `look at <container>` (or `open`) clears it. Without the examine, the
# contents are not in scope at all ("I can't see that").
#
# --- Courtyard: the cat opens the mansion -----------------------------------
take rabbit leg
tell cat to open door
east
# --- Kitchen: beer from the fridge, roast the rabbit leg --------------------
south
east
open fridge
take beer
open oven
put rabbit leg in oven
take cooked rabbit leg
west
north
west
# --- Two hints: "my social security card" / "spark plug, pump and battery" --
give beer to oracle
give cooked rabbit leg to oracle
east
# --- Library: the rug hides the hatch, the password is the author's name ----
north
move rug
say ernest cline
down
find switch
take flashlight
up
south
# --- Root Cellar (dark): chanterelles for the third hint --------------------
south
turn on flashlight
down
look at bucket
take mushrooms
up
north
west
give mushrooms to oracle
east
# --- Study: pull the dragon's ruby eye, a secret compartment pops out -------
east
up
west
look at dragon head
take ruby
look at secret compartment
take code
east
down
west
# --- Airlock: the code opens the vault into the nukeproof shelter -----------
north
down
use code on dial
east
look at bunk beds
take key
east
find spark plug
take spark plug
west
west
up
south
# --- Master bedroom / bathroom: battery, pump, laxatives --------------------
up
open brown drawer
take vibrator
open vibrator
take battery
east
open cabinet
take pump
take laxatives
use battery on pump
west
down
# --- Out the back door, and spike the dog food ------------------------------
east
north
use key on door
north
north
northeast
take can of dog food
use laxatives on can of dog food
# --- The road west to the boathouse -----------------------------------------
west
west
west
north
northeast
west
use spark plug on jeep
north
use pump on kayak
# --- Across the Holy Lake of Yesla to Goblin Mountain -----------------------
north
west
west
southwest
southwest
south
southwest
southeast
northeast
give can of spiced dog food to beast
west
take card
east
# --- Back in the courtyard: work the dispenser, dose the Oracle -------------
east
up
east
switch on device
use card on device
west
down
west
give pill to oracle
west
