# Into the Dragon's Den (XanMag, "The Pixie", 2016, ASL v550) — WINNING
#   walkthrough, override-only (no published walkthrough exists). Comedic
#   Dr.-Seuss-rhyming-verse game with health/onhealthzero=bare `finish`.
# Ending: dragon slain with the poisoned golden tooth while asleep —
#   "Congrats to you, you're a winner." oracle reports state=Finished
#   errors=0, steps=41, zero health lost along this route (deterministic,
#   no RNG). Several wrong choices along the way call bare `finish` on an
#   instant death (Wilfred without pants, Sly Shay's 3rd wrong riddle
#   answer, throwing the rock/attacking with sword or unpoisoned tooth vs.
#   the dragon) — this script avoids all of them.
# Route: `leave` the intro room -> Thieves Den. `open dresser`/`take key`
#   (alias of "small key"); `use key on black chest` opens it and this
#   single action also destroys the other 4 (booby-trapped, decoy) chests
#   and reveals a mouth/cavity/monk/notch/charms puzle — do NOT use the
#   found "another key" on anything but the cavity (the other targets cost
#   health for nothing): `use another key on cavity` reveals the door and
#   makes the (initially invisible) east exit to the break room visible.
#   East to the break room: Sly Shay guards it with a 3-strikes riddle
#   gauntlet (wrong x3 = death) — her FIRST riddle (asked on the very first
#   `talk to sly shay`) is "You'll say yes and I'll say no... which eludes
#   me to this song" = the Beatles' "Hello, Goodbye"; answering it right on
#   the first try skips the other two (harder, riskier) riddles entirely.
#   `take pants`/`wear pants` from the chair (both require the "correct"
#   flag Shay's defeat sets). West back to Thieves Den, `talk to wilfred`
#   now that pants are worn (else instant death) — this unlocks the north
#   exit to the forest room (a hub with paths to the saloon/cave/dragon).
#   West to the saloon (a yes/no gate, always re-prompts — answer yes):
#   north to the loo, `look at john` (hidechildren container) reveals
#   money, `take money` converts it to a "bill"; south back to the saloon,
#   `shout at bartender` trades the bill for a drink ("swill"), `drink
#   swill` sets the "accepted" flag; `sit in chair` (now accepted) seats
#   you at the card table and reveals something odd about the floor;
#   `look at floor` while seated auto-grabs a hidden ace; `talk to
#   patrons` (now seated, holding the ace) wins their card game "Teeter",
#   scaring them off and revealing a steak; `take steak`. East to the
#   forest room (entering with the steak locks the saloon exit and reveals
#   a rock) — `take rock`/`use rock on steak` loads it. North to the cave
#   (another yes/no gate, answer yes): `give steak to bear` (with the rock
#   inside) knocks a golden tooth loose; `take golden tooth`. South back to
#   the forest room (entering with the tooth locks the cave exit and
#   reveals a sword) — `take sword`. East to the dragon's porch (now
#   showing a nest of adders blocking the path north): `use sword on
#   adders` clears them and reveals a pool of venom; `use golden tooth on
#   pool of venom` poisons the tooth. North into the dragon's den: `sing`
#   puts the dragon to sleep; `use golden tooth on dragon` (poisoned +
#   asleep) delivers the killing blow and wins the game.
leave
open dresser
take key
use key on black chest
use another key on cavity
east
talk to sly shay
hello, goodbye
take pants
wear pants
west
talk to wilfred
north
west
yes
north
look at john
take money
south
shout at bartender
drink swill
sit in chair
look at floor
talk to patrons
yes
take steak
east
take rock
use rock on steak
north
yes
give steak to bear
take golden tooth
south
take sword
east
use sword on adders
use golden tooth on pool of venom
north
sing
use golden tooth on dragon
