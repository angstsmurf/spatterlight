# The Acreage (pub 6.29 revision) — D.C. Tozier (ASL 580).  Sci-fi narrative.
# The author's LATER build of a game already in the corpus: same
# <gameid> 2dc9c7e3-1eec-4737-b6e7-18fe94daefbc, saved by Quest
# 5.10.9635.26171 against the frozen row's 5.9.9166.36226, 177 objects
# against 173 (+Apartments, +Ships, +Spices, +couple).  The frozen row stays
# on the older build; this is its own row, as with the Baleful Backwash
# re-release.
#
# Same 93-step route, same COMPLETE GOOD-PATH WIN: the nuns collect Isaac on
# Surst with Desmond still at your side, you are paid, "THANKS FOR PLAYING",
# then the game's own `quit` command (-> finish), since the ending never
# calls finish itself (Serpent's Eye precedent).
#
# TWO DIFFERENCES FROM THE FROZEN ROW, and both matter.
#
# 1.  The Port recursion is mostly FIXED.  The older build needed
#     `#!errorlimit=200` because every Port entry re-ran the Port's
#     description into QuestViva's 200-depth cap, logging ~20-40 script
#     errors per visit (58 over the run).  Here the run logs 8, all of them
#     the surviving grid-coordinate lookups ("Error evaluating expression
#     'DictionaryItem(coordinates, coordinate)': The given key 'x'/'y'/'z'
#     was not present in the dictionary"), so no directive is needed.  They
#     come from the game's own copy of the grid-map library reading an unset
#     coordinate for TerminalRoom (no exits, so nothing ever sets one), and
#     they are the one place native `aslx_replay` does NOT match the oracle
#     byte for byte: it logs 5 of the 8.  That is by design, not a bug — the
#     native core only evaluates the arguments of an unimplemented `JS.*`
#     call when a host hook is installed, and headless there is no grid hook,
#     so `JS.Grid_DrawBox(x, y, z, ...)`'s three lookups never run.  Every
#     line of game text is identical.
#
# 2.  Desmond's conversation lost a topic.  "What kind of place is Surst?"
#     is gone from the bar menus (pre-beer 4 entries against 5, post-beer 5
#     against 6) and asking a topic now REMOVES it, which the older build did
#     not always do.  The frozen script's `3` still lands (on "Who is Isaac?"
#     rather than "What do you know about The Divide?"), but its following
#     `5` `5` are off the end of the menu.
#
#     That is the whole failure, and it is invisible: an out-of-range answer
#     leaves the ShowMenu OPEN and SILENTLY SWALLOWS every command after it.
#     Replaying the frozen script here produces no "I don't understand", no
#     "I can't see that" and errors=0 — it simply never leaves Notre Feu, the
#     port/office endgame is eaten one line at a time by the still-open menu,
#     and the run walks outside and quits.  The tell is a `>` echo with no
#     response, not an error message.  Fixed by asking "Who is Isaac?",
#     "Where did you find Isaac?", EXIT (`3` `3` `3`) — the same two stories
#     the frozen row hears, in the order this build offers them.
#
# Everything else replays unchanged: the treatment syringe always shatters
# (scripted) -> essage canister from the squatters' crate behind the chained
# clinic; Isaac pays for the Repair Kit at Colette's pawnshop; the kit fixes
# the motel manager's robot CAT, earning the referral to engineer Julia; the
# grocery-corner FOOD TOKEN given to hungry Shanti buys the drink token AND
# Colette's trust (she then sells the Suprabarbital pill Isaac demands); at
# the bar the pill is emptied ONTO THE FLOOR (defying Isaac) and the honest
# beer keeps Desmond loyal.  Part Two: revisiting the port while Julia works
# triggers the GasTechnin SECRETARY's debt-scam (remote ship lockdown); the
# rep is shot with the pistol, the office terminal's PORT CONTROLS lift the
# flight restrictions, and with the ship fixed and Desmond neither drugged
# nor signed away he boards as a loyal follower.  TAKE OFF -> ENGAGE FOLD
# ENGINES -> LANDING SEQUENCES -> exit east into the Surst-with-Desmond
# ending.  Conversations are numbered ShowMenu topic menus (bare numbers).
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
3
3
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
