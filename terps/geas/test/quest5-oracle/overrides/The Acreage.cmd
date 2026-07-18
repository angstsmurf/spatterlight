#!errorlimit=200
# The Acreage — D.C. Tozier, 2025 (ASL 580). Sci-fi narrative.
# No published walkthrough exists — derived from game source (game.aslx).
# COMPLETE GOOD-PATH WIN: ends on Surst with Desmond still at your side —
# the nuns collect Isaac, you are paid, "THANKS FOR PLAYING" — then the
# game's own `quit` command (-> finish), since the ending never calls finish
# itself (Serpent's Eye precedent).
# The `#!errorlimit=200` directive is REQUIRED: every Port entry re-runs the
# Port's room description into QuestViva's 200-depth recursion cap (the
# description's own MovePlayer(Port)/grid-coordinate machinery), racking up
# ~20-40 logged script errors per visit — 58 over the full run. Interactive
# play hides the loop behind the description's leading ClearScreen; legacy
# Quest had no error breaker at all, so raising the qvh/native breaker limit
# is the faithful reading. All 58 errors are the game's own, reproduced
# byte-identically by the native engine.
# Part One beats: the treatment syringe always shatters (scripted) → fetch the essage
# canister from the squatters' crate behind the chained clinic; Isaac pays
# for the Repair Kit at Colette's pawnshop; the kit actually fixes the motel
# manager's robot CAT, earning the referral to engineer Julia (kitchen of the
# company bar, unlocked by that knowledge); the grocery-corner FOOD TOKEN,
# given to hungry Shanti, buys the drink token AND Colette's trust (she then
# sells the Suprabarbital pill Isaac demands); at the bar the pill is emptied
# ONTO THE FLOOR (defying Isaac) and the honest beer keeps Desmond loyal.
# Part Two: revisiting the port while Julia works triggers the GasTechnin
# SECRETARY's debt-scam (remote ship lockdown); Isaac demands blood — the
# rep is shot with the pistol (it shatters; he falls on his keyboard), the
# office terminal's PORT CONTROLS lift the flight restrictions, and with the
# ship fixed and Desmond neither drugged nor signed away he boards as a
# loyal follower. TAKE OFF -> ENGAGE FOLD ENGINES (the fold check auto-locks
# the broken firearm away) -> LANDING SEQUENCES -> exit east into the
# Surst-with-Desmond ending. Conversations are numbered ShowMenu topic menus
# (answers are bare numbers; they renumber as topics hide).
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
e
n
s
1
2
e
talk to rep
2
x terminal
2
2
1
3
w
n
e
x terminal
1
x terminal
2
x terminal
3
e
quit
