# The Acreage — D.C. Tozier, 2025 (ASL 580). Sci-fi narrative.
# No published walkthrough exists — derived from game source (game.aslx).
# BEST-EFFORT: the story requires re-entering the Acreage Port while Julia
# repairs The Vigil, and that entry re-runs the Port's room description in an
# unbounded loop (the description's own MovePlayer(Port) / grid-coordinate
# machinery recurses to QuestViva's 200-depth cap over and over). Interactive
# play hides this behind the description's leading ClearScreen, but the
# repeated depth-cap throws trip QuestViva's 20-script-error breaker =
# Wedged. This script therefore plays PART ONE completely and parks at its
# natural end (Desmond happily drunk on an undrugged beer — the "good path"
# resolution of his arc) without entering the Port again. Even the single
# mandatory first Port visit (x ship) racks up 20 NON-fatal depth errors
# (logged via the display path, Bony King-style, so the breaker stays unfired).
# Beats: the treatment syringe always shatters (scripted) → fetch the essage
# canister from the squatters' crate behind the chained clinic; Isaac pays
# for the Repair Kit at Colette's pawnshop; the kit actually fixes the motel
# manager's robot CAT, earning the referral to engineer Julia (kitchen of the
# company bar, unlocked by that knowledge); the grocery-corner FOOD TOKEN,
# given to hungry Shanti, buys the drink token AND Colette's trust (she then
# sells the Suprabarbital pill Isaac demands); at the bar the pill is emptied
# ONTO THE FLOOR (defying Isaac) and the honest beer keeps Desmond loyal.
# Conversations are numbered ShowMenu topic menus (answers are bare numbers).
w
take vial
take syringe
e
e
n
n
se
s
w
x crate
1
take canister
e
n
nw
s
s
w
e
n
w
talk to clerk
5
5
e
nw
x newspaper
take coin
se
n
n
x ship
s
e
w
s
s
talk to manager
1
2
3
n
n
w
w
talk to julia
1
e
talk to shanti
1
e
s
w
talk to clerk
5
5
e
n
w
talk to bartender
1
2
talk to desmond
1
talk to desmond
3
5
5
