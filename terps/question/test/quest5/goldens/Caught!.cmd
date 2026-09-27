# Caught! - Escape the Noose — T H Duncan, 2016 (ASL 550)
# No published walkthrough exists — derived from game source (game.aslx).
# Prison-escape with health + score; "There are three ways to win" (window,
# moat, drawbridge password) — this script takes the WINDOW escape (endgood,
# the biggest score bonus). Chain: `search straw` finds the cell key; the
# flat stone in Cell 2's lidded bucket carries the moat clue, read by
# `use torch on flat stone` in the lit Hallway (+6); rusty knife from the
# Midden Pit cuts the kitchen rope — the falling pot rack KOs the cook
# (his call-the-guard SetTimeouts are disarmed before the oracle drains
# them); cook's hat+apron disguise + food tray gets you across the Dining
# Hall (the guards take the tray and send you to the Master); the Lobby rat
# is scooped into the chef's hat; in the Master Bedroom the master closes in
# on a 4-turn kill timer — `use rat on master` on turn 1 KOs him (he is
# terrified of rats), dropping the letter opener that cuts the curtain rope
# (rope length 2 = kitchen rope + curtain rope); `open window` ties them and
# slides down: "You won! Congratulations!" → finish. The desk book's
# password list ("Excalibur", for the drawbridge ending) is read as flavour.
# The wandering-guard timer is recurring (dormant headless); all cited
# SetTimeouts are drained deterministically. No RNG on this path.
# The kitchen can only be entered safely in the GUARD uniform (trousers from
# chest 2 + jacket from chest 4 — exactly the two chest-opens the creaking
# allows; the third summons the guard, and the boots are a -10-health trap);
# undisguised entry arms the cook's call-the-guard SetTimeout chain, which
# the oracle's drain fires instantly = caught. The guard uniform is dropped
# again in the kitchen (the cook demands an assistant in COOK's clothes).
# Inventory is capped at 5 objects — the spent flat stone is dropped to make
# room for the tray.
search straw
unlock door
w
w
open bucket
x flat stone
take flat stone
e
use torch on flat stone
se
take knife
nw
sw
w
open far right chest
take trousers
wear trousers
open near right chest
take guards jacket
wear guards jacket
w
n
cut rope
drop trousers
drop guards jacket
open cupboard
wear apron
take length of rope
drop flat stone
take tray
eat food
s
w
w
s
catch rat
n
n
use rat on master
take letter opener
open desk
read book
cut yellow rope
open window
