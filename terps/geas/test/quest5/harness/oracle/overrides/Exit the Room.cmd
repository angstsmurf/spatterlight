# Exit the room — jerkdavi, 2021 (ASL 580, Quest 5.8)
# No published walkthrough exists — derived from game source (game.aslx).
# Single-room escape game built on a hand-rolled two-letter flag machine
# (knck as->at->au->av etc.). Ending: `knock on door` once knck reaches "av"
# — set only after BOTH (a) `read matthew 7:7-8` (requires the Bible in hand
# and the rd chain: x note ["7:7-8" scribble] + x icon [Matthew is your
# favourite saint], in either order) and (b) the copykey failing in the lock
# (`use copykey on door`). The faith moral: keys never work, knocking does
# ("...knock and the door will be opened to you...") -> THE END + finish.
# copykey is found via `compare doorkey to keys` (get-input "yes" answers the
# are-you-sure prompt; the 20-key counting wait{} chain auto-continues).
# The doorkey must be taken from the lock first (x door clears hidechildren);
# the box appears only after `move tunic`, must be taken OUT of the closet to
# open, and hides the keys ring. 100-turn insanity limit (finish = game over)
# — this script is 21 turns; the recurring Clang timers stay dormant headless.
# The game HidePreviousTurnOutput's every turn (visual only). No score/RNG.
x door
take doorkey
x bed
look under pillow
take note
x note
x icon
x bookcase
take Bible
read matthew 7:7-8
open closet
x closet
move tunic
take box
open box
x box
take keys
compare doorkey to keys
yes
take copykey
use copykey on door
knock on door
